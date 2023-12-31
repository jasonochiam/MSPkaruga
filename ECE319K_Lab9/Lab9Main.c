// Lab9Main.c
// Runs on MSPM0G3507
// Lab 9 solution
// Aidan Aalund and Jason Ochiam
// Last Modified: 11/13/2023

#include <stdio.h>
#include <stdint.h>
#include <ti/devices/msp/msp.h>
#include "../inc/ST7735.h"
#include "../inc/Clock.h"
#include "../inc/LaunchPad.h"
#include "../inc/TExaS.h"
#include "../inc/Timer.h"
#include "ADC1.h"
#include "ADC.h"
#include "DAC5.h"
#include "FIFO1.h"
#include "UART1.h"
#include "UART2.h"
#include "SmallFont.h"
#include "LED.h"
#include "Switch.h"
#include "Sound.h"
#include "JoyStick.h"
#include "images/images.h"


// Random defines go here
// LED defines
#define LEFT 1<<24
#define MID 1<<27
#define RIGHT 1<<28
#define FIX 3
#define GREENWAVE 0
#define YELLOWWAVE 1

// misc defines
#define NUMCOLORS 2
#define NUMHP 4 // it is actually the number you put - 1, we account for not rendered states

//Flags
uint32_t Flag = 0; // Semaphore
uint32_t Language; // 0 for english, 1 for spanish
uint32_t title = 1; // flag to indicate if the game is on the title screen or tutorial.
uint32_t tutorial = 0; // flag to indicate if the game in the tutorial

// Inputs
uint32_t XData; // Variable that holds X position of stick. Units: 0-4095
uint32_t YData; // Variable that holds Y position of stick. Units: 0-4095
int32_t x; // Signed version of XData, needed to work with drivers
int32_t y; // Signed version of YData, needed to work with drivers
uint32_t shoots; // holds the left button input for this frame
uint32_t lastshoot; // holds the left button input for the last frame
uint32_t selects; // holds the right button input for this frame
uint32_t lastselects; // holds the right button input for the last frame
uint32_t swaps; // holds the bottom button input the this frame
uint32_t lastswaps; // holds the bottom button input for the last frame
uint32_t ups; // holds the top button input for this frame
uint32_t lastups; // holds the top button input for the last frame
uint32_t click; // holds the joystick click for this frame

// Game state flags
uint32_t redrawplayer; // indicates to the render system if the player must be redrawn
uint32_t redrawbg = 1; // tells the game to redraw the background (needed for title screen logic)
uint32_t timer; // holds the current time, wonderful. Units: 33.3ms
uint32_t titletimer; // holds the current time for the title screen. Units: 33.3ms
uint32_t score; // the score for the level. Units: points
uint32_t end; // tells the game to end or not (slightly different than win)
uint8_t win; // indicates if the player has won the game (1 yes, 0 not yet)
uint32_t wave = 0;
uint32_t first = 1;
uint32_t titleflag = 0; // poorly named, but indicates to the title screen to show the second part of a phrase
uint32_t bosscolor = 1;
uint32_t bosskilled = 0;
int32_t shiftvelocity = 1;

// Misc
const uint16_t *spaceptr = space; // pointer to space image
const uint16_t *titleptr = titlescreen; // pointer to title image
const uint16_t *valvanoptr = valvanosad;
uint16_t textcolor = 0;

// ****note to ECE319K students****
// the data sheet says the ADC does not work when clock is 80 MHz
// however, the ADC seems to work on my boards at 80 MHz
// I suggest you try 80MHz, but if it doesn't work, switch to 40MHz
void PLL_Init(void){ // set phase lock loop (PLL)
  // Clock_Init40MHz(); // run this line for 40MHz
  Clock_Init80MHz(0);   // run this line for 80MHz
}

uint32_t M=1;
uint32_t Random32(void){
  M = 1664525*M+1013904223;
  return M;
}
uint32_t Random(uint32_t n){
  return (Random32()>>16)%n;
}

// Sprite structure
struct sprite{
    uint32_t life; // 0 dead, 1 alive, 2 is dying (will replace with 0 dead, 1 dying, >1 alive)
    uint32_t color; // 0 is green, 1 is yellow (use this for checking bullet-player collisions AFTER you know a hit has occurred)
    uint32_t type; // used to determine enemies. 1 is basic enemy (no shots), 2 fires lasers straight down periodically, 3 fires lasers toward enemy, and 4 is boss.
                   //This number can be used as an index to select a sprite. Multiply this number by 10 to calculate score for killing it.
    int32_t x,y; // positive lower left corner. Units: pixels>>FIX
    int32_t lastx,lasty; // used to avoid sprite flicker. Units: pixels>>FIX
    uint8_t enemylaser;   //checks for laser time, 0 for player, 1 for enemy
    uint32_t tracking; // set for lasers to see if shots will track
    uint32_t spawntime;
    uint32_t iframe;
    uint32_t invincible;
    uint8_t reversedir;    //used to indicate direction, if 1, then enemy moves right first
    const uint16_t *redimage;
    const uint16_t *greenimage;
    const uint16_t *image[NUMCOLORS][NUMHP+1]; // a 2d array of images. X axis is damage level and Y axis is color
//    const uint16_t *image2; // the default image for the sprite, in cases where I haven't implemented multiple damage images
    const uint16_t *blankimage; // the image to render over where the sprite was (needed for fast movement)
    const uint16_t *swapanimation; // points to swap animation frames for the player
    // TODO: more image pointers as needed for animation frames, other status values
    int32_t w,h; // the width and height of the sprite. Units: pixels
    int32_t vx,vy; // the velocity of the sprite. Units: pixels>>FIX per second
};

typedef struct sprite sprite_t;

#define NUMENEMIES 50
sprite_t enemy[NUMENEMIES];
#define NUMLASERS 50
sprite_t lasers[NUMLASERS];
#define NUMMISSILES 50
sprite_t missiles[NUMMISSILES];
sprite_t player;

// Enemy pattern, spawns two small enemies at the top of the screen. Not using in final game!
void enemyball(sprite_t enemy);
void enemylaser(sprite_t enemy);

void enemy_init(void){
    for(int i = 0; i<2; i++){
        enemy[i].life = 2; // spawn an enemy at (n-1) hp
        enemy[i].x = (32+32*i)<<FIX;
        enemy[i].y = 10<<FIX;
//        enemy[i].image2 = SmallEnemy10pointA;
        enemy[i].blankimage = SmallEnemy10pointAblank;
        enemy[i].vx = 0;
        enemy[i].vy = 1; // NOTE: this is 1<<FIX pixel per frame moving DOWN.
        enemy[i].w = 16;
        enemy[i].h = 10;
    }
}

void spawnsmallenemy(uint32_t x,uint32_t y, int32_t vx, int32_t vy, uint32_t hp, uint32_t color, uint32_t type){
// search the enemy array for an un-spawned enemy
// TODO: what to do when enemy cap reached? replace oldest enemy
    for(int i = 0; i<NUMENEMIES; i++){
        // set the first one you find to all the input parameters
        if(enemy[i].life == 0){
            enemy[i].life = hp+1; // effective starting hp values
            enemy[i].color = color;
            enemy[i].x = x;
            enemy[i].y = y;
            enemy[i].image[0][0] = SmallEnemy10pointGreenA;
            enemy[i].image[1][0] = SmallEnemy10pointYellowA;
            enemy[i].blankimage = SmallEnemy10pointAblank;
            enemy[i].vx = vx;
            enemy[i].vy = vy;
            enemy[i].w = 16;
            enemy[i].h = 10;
            enemy[i].type = type;
//            enemylaser(enemy[i]);
//            enemyball(enemy[i]);
            break;
        }

    }
    // if it is full, do nothing
}

// alternate enemy type that spawns
void spawnmediumenemy(uint32_t x,uint32_t y, int32_t vx, int32_t vy, uint32_t hp, uint32_t color){
     for(int i = 0; i<NUMENEMIES; i++){
         // set the first one you find to all the input parameters
         if(enemy[i].life == 0){
             enemy[i].life = hp+1; // effective starting hp values
             enemy[i].color = color;
             enemy[i].x = x;
             enemy[i].y = y;
             enemy[i].image[0][0] = SmallEnemy20pointGreenA;
             enemy[i].image[1][0] = SmallEnemy20pointYellowA;
//             enemy[i].image2 = SmallEnemy10pointA;
             enemy[i].blankimage = SmallEnemy10pointAblank;
             enemy[i].vx = vx;
             enemy[i].vy = vy;
             enemy[i].w = 16;
             enemy[i].h = 10;
             enemy[i].type = 2;
//             enemyball(enemy[i]);
             break;
         }
     }
}

void spawnboss(uint32_t x, uint32_t y, int32_t vx, int32_t vy, uint32_t hp, uint32_t color){
     for(int i = 0; i<NUMENEMIES; i++){
         // set the first one you find to all the input parameters
         if(enemy[i].life == 0){
             enemy[i].life = hp+1; // effective starting hp values
             enemy[i].color = color; // boss can switch color
             // position wil probably be fixed, may remove parameters
             enemy[i].x = x;
             enemy[i].y = y;
             // TODO: change these when boss image done
             enemy[i].image[0][0] = AlienBossABigGreen24;
             enemy[i].image[1][0] = AlienBossABigYellow24;
             enemy[i].blankimage = AlienBossABigEmpty24;
             enemy[i].vx = 0; // likely 0
             enemy[i].vy = 0; // likely 0
             // TODO: change these when boss image done
             enemy[i].w = 48;
             enemy[i].h = 24; //20
             enemy[i].type = 4;
             break;
         }
     }
}

// ENEMY SHOOT SPAWNS
void bosslaser(uint32_t x,uint32_t y, int32_t vx, int32_t vy, uint32_t hp, uint32_t color){
    for(int i = 0; i<NUMMISSILES; i++){
        if(missiles[i].life == 0){
            missiles[i].life = 1;     //1 for alive and moving, 2 for collision, 0 for despawned
            missiles[i].x = x;   //start at center of player
            missiles[i].y = y;
            missiles[i].color = color;
            missiles[i].image[0][0] = LaserGreen0;
            missiles[i].image[1][0] = LaserYellow0;
            missiles[i].blankimage = eLaser0;
            // math: player x - enemy x / time to travel
            // make x velocity sinusoidal?
            missiles[i].vx = vx;
            missiles[i].vy = vy; // NOTE: -10 is 1 pixel per frame moving DOWN.
            missiles[i].w = 2;
            missiles[i].h = 9;
            missiles[i].spawntime = timer;
            missiles[i].tracking = 0;
            missiles[i].enemylaser = 1;
            missiles[i].type = 0;
            i++;
            if(i == NUMMISSILES){
                i = 0;
            }
            break;
        }
    }
}

void bossball(uint32_t x,uint32_t y, int32_t vx, int32_t vy, uint32_t hp, uint32_t color){
    for(int i = 0; i<NUMMISSILES; i++){
        if(missiles[i].life == 0){
            missiles[i].life = 1;     //1 for alive and moving, 2 for collision, 0 for despawned
            missiles[i].x = x;   //start at center of player
            missiles[i].y = y;
            missiles[i].color = color;
            missiles[i].image[0][0] = GreenBall;
            missiles[i].image[1][0] = YellowBall;
            missiles[i].blankimage = eBall;
            // math: player x - enemy x / time to travel
            // make x velocity sinusoidal?
            missiles[i].vx = vx;
            missiles[i].vy = vy; // NOTE: -10 is 1 pixel per frame moving DOWN.
            missiles[i].w = 14;
            missiles[i].h = 13;
            missiles[i].spawntime = timer;
            missiles[i].tracking = 0;
            missiles[i].enemylaser = 1;
            missiles[i].type = 1;
            i++;
            if(i == NUMMISSILES){
                i = 0;
            }
            break;
        }
    }
}

//spawnboss(40<<FIX, 30<<FIX, 0, 0, 10, 1);
void bossattack(sprite_t enemy){     //will initialize a new bullet specifically if called for enemy sprite
    int32_t width = (enemy.w<<FIX)/2;
//    bosslaser(enemy.x+width-(10<<FIX),enemy.y-(5<<FIX), 0, 5, 1, enemy.color);
//    bosslaser(enemy.x+width,enemy.y, 0, 5, 1, enemy.color);
//    bosslaser(enemy.x+width+(10<<FIX),enemy.y-(5<<FIX), 0, 5, 1, enemy.color);
    bossball(enemy.x+width-(20<<FIX),enemy.y, 0, 6, 1, enemy.color);
    bossball(enemy.x+width-(40<<FIX),enemy.y, 0, 6, 1, enemy.color);
    bossball(enemy.x+width,enemy.y, 0, 6, 1, enemy.color);
    bossball(enemy.x+width+(20<<FIX),enemy.y, 0, 6, 1, enemy.color);
    bossball(enemy.x+width+(40<<FIX),enemy.y, 0, 6, 1, enemy.color);
}



// spawns a ball starting at the provided enemy, and tracks the player for 5 seconds.
void enemyball(sprite_t enemy){     //will initialize a new bullet specifically if called for enemy sprite
    for(int i = 0; i<NUMMISSILES; i++){
        if(missiles[i].life == 0){
            missiles[i].life = 1;     //1 for alive and moving, 2 for collision, 0 for despawned
            missiles[i].x = enemy.x;   //start at center of player
            missiles[i].y = enemy.y + (13<<FIX);
            missiles[i].color = enemy.color;
            missiles[i].image[0][0] = GreenBall;
            missiles[i].image[1][0] = YellowBall;
            missiles[i].blankimage = eBall;
            // math: player x - enemy x / time to travel
            // make x velocity sinusoidal?
            missiles[i].vx = (player.x - enemy.x)/15;
            missiles[i].vy = (player.y - enemy.y)/15; // NOTE: -10 is 1 pixel per frame moving DOWN.
            missiles[i].w = 14;
            missiles[i].h = 13;
            missiles[i].spawntime = timer;
            missiles[i].tracking = 1;
            missiles[i].enemylaser = 1;
            missiles[i].type = 1;
            i++;
            if(i == NUMMISSILES){
                i = 0;
            }
            break;
        }
    }
}

void spawnshiftenemy(uint32_t x,uint32_t y, int32_t vx, int32_t vy, uint32_t hp, uint32_t color, uint8_t dir){
// search the enemy array for an un-spawned enemy
// TODO: what to do when enemy cap reached? replace oldest enemy
    for(int i = 0; i<NUMENEMIES; i++){
        // set the first one you find to all the input parameters
        if(enemy[i].life == 0){
            enemy[i].life = hp+1; // effective starting hp values
            enemy[i].color = color;
            enemy[i].x = x;
            enemy[i].y = y;
            enemy[i].image[0][0] = SmallEnemy10pointGreenA;
            enemy[i].image[1][0] = SmallEnemy10pointYellowA;
            enemy[i].blankimage = SmallEnemy10pointAblank;
            enemy[i].vx = 0;
            enemy[i].vy = vy;
            enemy[i].w = 16;
            enemy[i].h = 10;
            enemy[i].type = 3;
            if(dir == 1){
                enemy[i].reversedir = 1;
            }
            if(dir == 2){
                enemy[i].reversedir = 2;
            }
            break;
        }

    }
    // if it is full, do nothing
}

void enemylaser(sprite_t enemy){     //will initialize a new bullet specifically if called for enemy sprite
    for(int i = 0; i<NUMMISSILES; i++){
        if(missiles[i].life == 0){
            missiles[i].life = 1;     //1 for alive and moving, 2 for collision, 0 for despawned
            missiles[i].w = 2;
            missiles[i].h = 9;
            missiles[i].x = enemy.x+((enemy.w/2)<<FIX);   //start at center of player
            missiles[i].y = enemy.y + (enemy.h<<FIX);
            if(missiles[i].y > 159<<FIX){
                missiles[i].life = 0;
                break;

            }
            missiles[i].color = enemy.color;
            missiles[i].image[0][0] = LaserGreen0;
            missiles[i].image[1][0] = LaserYellow0;
            missiles[i].blankimage = eLaser0;
            missiles[i].vx = 0;
            missiles[i].vy = 1<<FIX; // NOTE: -10 is 1 pixel per frame moving DOWN.
            missiles[i].enemylaser = 1; // THIS IS USED DO NOT DELETE
            missiles[i].tracking = 0;
            i++;
            if(i == NUMMISSILES){
                i = 0;
            }
            break;
         }
    }
}


// ENEMY FORMATION SPAWNS
// TODO: at a wave spawn rate of 1 every 9 seconds, we need 13 or so enemies

// spawns a line of basic enemies
void spawnline(uint32_t color){
    spawnsmallenemy(16<<FIX,9<<FIX,0,2,1,color,0);
    spawnsmallenemy(36<<FIX,9<<FIX,0,2,1,color,1);
    spawnsmallenemy(56<<FIX,9<<FIX,0,2,1,color,0);
    spawnsmallenemy(76<<FIX,9<<FIX,0,2,1,color,1);
    spawnsmallenemy(96<<FIX,9<<FIX,0,2,1,color,0);
}

void spawnbird(){
    spawnsmallenemy(16<<FIX,9<<FIX,0,2,1,1,1);
    spawnsmallenemy(36<<FIX,19<<FIX,0,2,1,0,1);
    spawnsmallenemy(56<<FIX,29<<FIX,0,2,1,1,1);
    spawnsmallenemy(76<<FIX,19<<FIX,0,2,1,0,1);
    spawnsmallenemy(96<<FIX,9<<FIX,0,2,1,1,1);
}

// 3 evenly spaced enemies that race down the screen. Use with other enemy formations
void spawnfastline(uint32_t color, uint32_t y){
    spawnsmallenemy(16<<FIX,y,0,4,1,color,0);
    spawnsmallenemy(36<<FIX,y,0,4,1,color,1);
    spawnsmallenemy(56<<FIX,y,0,4,1,color,0);
    spawnsmallenemy(76<<FIX,y,0,4,1,color,1);
    spawnsmallenemy(96<<FIX,y,0,4,1,color,0);
}

// ideally this circle rotates around you
// void spawncircle()

// 2-4 enemies that start from bottom corners and race to the top
void spawnx(uint32_t color){
    spawnsmallenemy(8<<FIX,10<<FIX,4,5,1,color,1);
    spawnsmallenemy(8<<FIX,25<<FIX,4,5,1,color,1);

    spawnsmallenemy(8<<FIX,130<<FIX,4,-5,1,color,1);
    spawnsmallenemy(8<<FIX,150<<FIX,4,-5,1,color,1);

    spawnsmallenemy(108<<FIX,10<<FIX,-4,5,1,color,1);
    spawnsmallenemy(108<<FIX,25<<FIX,-4,5,1,color,1);

    spawnsmallenemy(108<<FIX,135<<FIX,-4,-5,1,color,1);
    spawnsmallenemy(108<<FIX,150<<FIX,-4,-5,1,color,1);
}


// PLAYER FUNCTIONS

// initialize player
// WARNING: THE PLAYER HEALTH SYSTEM WORKS DIFFERENT THEN ENEMY SYSTEM. IM SORRY MY CODE IS BAD
void player_init(void){
    player.x = player.lastx = 60<<FIX;
    player.y = player.lasty = 159<<FIX;
    player.life = NUMHP; // 4 is 3/3 hp, 3 is 2/3 hp, 2 is 1/3 hp, 1 is dying, 0 is dead (despawned)
    player.image[0][0] = PlayerShip0;
    player.image[0][1] = PlayerShip1;
    player.image[0][2] = PlayerShip2;
    player.image[0][3] = PlayerShip3;
    player.image[0][4] = PlayerShip4;
    player.image[1][0] = PlayerShipYellow0;
    player.image[1][1] = PlayerShipYellow1;
    player.image[1][2] = PlayerShipYellow2;
    player.image[1][3] = PlayerShipYellow3;
    player.image[1][4] = PlayerShip4;
    player.blankimage = PlayerShip4;
    player.w = 18;
    player.h = 8;
}


// uses the player's life to update PCB LEDs
void ledstatus(void){
    switch(player.life){
    case 4:
        LED_On(LEFT);
        LED_Off(MID);
        LED_Off(RIGHT);
        break;
    case 3:
        LED_Off(LEFT);
        LED_On(MID);
        LED_Off(RIGHT);
        break;
    case 2:
        LED_Off(LEFT);
        LED_On(MID);
        LED_Off(RIGHT);
        break;
    case 1:
        LED_Off(LEFT);
        LED_Off(MID);
        LED_On(RIGHT);
        break;
    default:
        break;
    }
}

void lasers_init(void){     //will initialize a new bullet every time switch is pressed with a max of 20 bullets on screen at once
    static uint8_t i = 0;
    if(i < NUMLASERS){
        lasers[i].life = 1;     //1 for alive and moving, 2 for collision, 0 for despawned
        lasers[i].color = player.color;
        lasers[i].image[0][0] = LaserGreen0;
        lasers[i].image[1][0] = LaserYellow0;
        lasers[i].blankimage = eLaser0;
        lasers[i].vx = 0;
        lasers[i].vy = -20; // NOTE: -10 is 1 pixel per frame moving DOWN.
        lasers[i].w = 2;
        lasers[i].h = 9;
        lasers[i].x = player.x+(player.w<<(FIX-1));   //start at center of player
        lasers[i].y = player.y-lasers[i].h;
        lasers[i].enemylaser = 0;       //type = player bullets

        i++;
        if(i == NUMLASERS){
            i = 0;
        }
    }
}

void changecolor(void){
    player.color++;
    //player.color &= 0x1;
    if(player.color > 1){
        player.color = 0;
    }
}

// moves all enemies
void move(void){
    // Sample the joystick
    JoyStick_In(&XData,&YData); // XData is 0 to 4095, higher is more to the left
                                // YData is 0 to 4095, higher is further up

    // Dead-zone has a radius of 100
    // At the center, the Y value like to hang from 2080 to 2090
    // For X, 2010 to 2020
    x = XData-2010; // makes middle close to zero, left is positive, right is negative
    y = YData-2080;
    // Deadzone handler
    player.vx = (x>>7);
    player.vy = (y>>7);
    if((-100<x)&&(x<100)){
        player.vx = 0;
    }
    if((-100<y)&&(y<100)){
        player.vy = 0;
      }

    player.lastx = player.x;
    player.x -= player.vx;
    if(player.x > 110<<FIX){
        player.x = 110<<FIX;
    }
    if(player.x < 0){
        player.x = 0;
    }

    player.lasty = player.y;
    player.y -= player.vy;
    if(player.y > 159<<FIX){
        player.y = 159<<FIX;
    }
    if(player.y < 0){
        player.y = 0;
    }

    if(timer-player.iframe>=30){
        player.invincible = 0;
    }

    if(timer%60 == 1){
        shiftvelocity = -shiftvelocity;
    }

    // move lasers
    for(int j = 0; j < NUMLASERS; j++){
        if(lasers[j].life == 1){
            if(lasers[j].y >= 159<<FIX || lasers[j].y <= 0 || lasers[j].x >= 128<<FIX || lasers[j].x < 0){
                //if bullet is offscreen, despawn
                lasers[j].life = 2;
            }
            else{
                lasers[j].lastx = lasers[j].x;
                lasers[j].lasty = lasers[j].y;
                lasers[j].x += lasers[j].vx;
                lasers[j].y += lasers[j].vy;
            }
        }
    }

    // move missiles
    for(int j = 0; j < NUMMISSILES; j++){
        if(missiles[j].life == 1){
            if(missiles[j].y >= 159<<FIX || missiles[j].y <= 0 || missiles[j].x >= 128<<FIX || missiles[j].x < 0){
                //if bullet is offscreen, despawn
                missiles[j].life = 2;
            }
            else{
                if(missiles[j].tracking == 1){
                    if((timer - missiles[j].spawntime < 150)){
                        // (player-enemy) = velocity*time?
                        if(timer%6 == 1){
                            missiles[j].vx = (player.x - missiles[j].x)/15;
                            missiles[j].vy = (player.y - missiles[j].y)/15;
                        }

                    }
                    else{
                        missiles[j].life = 2;
                    }
                }
                missiles[j].lastx = missiles[j].x;
                missiles[j].lasty = missiles[j].y;
                missiles[j].x += missiles[j].vx;
                missiles[j].y += missiles[j].vy;
            }
        }
    }

    // move enemies, then run collision checks
    for(int i = 0; i<NUMENEMIES; i++){
            if(enemy[i].life > 1){     //check for bullet collision here with a nested for loop comparing dimensions of each bullet active and each enemy
                if(enemy[i].y >= 157<<FIX || enemy[i].x >= 128<<FIX || enemy[i].y <= 3<<FIX || enemy[i].x <= 0<<FIX){
                 // this is space invaders logic, enemies 'win' when they move to bottom
//                    enemy[i].life = 2;
                    enemy[i].life = 1;
//                    end = 1;    //used to end game in main if aliens win
                    Sound_Killed();

                }
                else{       //else move enemies
                    // IF the enemy is a shift type
                    if(enemy[i].type == 3){
                        if (enemy[i].reversedir == 1){
                            enemy[i].vx = -7*shiftvelocity;
                            enemy[i].vy = 0;
                        }
                        else if(enemy[i].reversedir == 2){
                            enemy[i].vx = 7*shiftvelocity;
                            enemy[i].vy = 0;
                        }
                        else{
                        enemy[i].vx = 2*shiftvelocity;
                        enemy[i].vy = 0;
                        }
                    }
                    enemy[i].lastx = enemy[i].x;
                    enemy[i].lasty = enemy[i].y;
                    enemy[i].x += enemy[i].vx;
                    enemy[i].y += enemy[i].vy;

                }


               for(int j = 0; j < NUMLASERS; j++){
                   if(lasers[j].life == 1){
                       // recall that the 'position'
                       if( (enemy[i].type == 4)
                               && ( lasers[j].x <= (enemy[i].x + (enemy[i].w<<FIX)) ) && (lasers[j].x >= (enemy[i].x))
                               && ( lasers[j].y <= (enemy[i].y - ((enemy[i].h/2)<<FIX))) && (lasers[j].y >= (enemy[i].y))){
                           enemy[i].life--;
                           if(enemy[i].life == 1){
                               score += (enemy[i].type*10)+10;
                           }
                           lasers[j].life = 2;
                       }
                       else if(lasers[j].life != 2 &&
                       ((lasers[j].x <= (enemy[i].x + (enemy[i].w<<FIX))) && (lasers[j].x >= (enemy[i].x)))
                               && ((lasers[j].y <= (enemy[i].y + (enemy[i].h<<FIX))) && (lasers[j].y >= (enemy[i].y)))

                      ){
                      // if collision occurred and color matched, kill the enemy. In all collisions despawn laser sprite.
                           if(lasers[j].color == enemy[i].color && lasers[j].enemylaser == 0){
                               enemy[i].life--;
                               if(enemy[i].life == 1){
                                   score += (enemy[i].type*10)+10;
                               }
                           }
                           else{
                               Sound_Explosion();
                           }
                           lasers[j].life = 2;
                       }
                   }
               }

               // Enemy missile on player collisions
               for(int u = 0; u<NUMMISSILES; u++){
                   if(missiles[u].life == 1){
                      // recall that the 'position' of a sprite is the top left corner
                      if((player.x-missiles[u].x)*(player.x-missiles[u].x)+(player.y-missiles[u].y)*(player.y-missiles[u].y) <= (600<<FIX)){
                         // checking for enemy bullet collision with player
                          if(player.color != missiles[u].color && !player.invincible){
                               player.life--;
                               missiles[u].life = 2;
                               player.invincible = 1;
                               player.iframe = timer;
                               if(player.life == 0){
                                   end = 1;
                                   // TODO: may be important later
                                   redrawbg = 1;
                               }
                               Sound_Explosion();
                          }
                          missiles[u].life = 2;
                     }
                  }
               }

               if( (player.x-enemy[i].x)*(player.x-enemy[i].x)+(player.y-enemy[i].y)*(player.y-enemy[i].y) <= (600<<FIX)
                       && !player.invincible){
                   // get rid of line below when i-frames are added
                   enemy[i].life = 1;
                   player.life--;
                   player.invincible = 1;
                   player.iframe = timer;
                   if(player.life == 0){
                       end = 1;
                       redrawbg = 1;
                   }
                   Sound_Explosion();
               }

            }
    }

    // Enemy shooting system
    for(int i = 0; i<NUMENEMIES; i++){
        if(enemy[i].life > 1){
            switch(enemy[i].type){
                        // this will be for passive enemies
                        case 0:
                            break;
                        // if enemy type is one, RNG shoot
                        //laser shooting breaks the system
                        case 1:
                            if(Random(100) == 1){
                                enemylaser(enemy[i]);
                            }
                            break;
                        // if enemy type is two, periodic shooting
                        case 2:
                            if(timer%80 == 1 && Random(2) == 1){
                                enemyball(enemy[i]);
                            }
                            break;
                        case 3:
                            if(Random(150) == 1){
                                enemylaser(enemy[i]);
                            }
                            break;
                        case 4:
                            if(timer % 60 == 1){
                                enemy[i].color = bosscolor;
                                bossattack(enemy[i]);
                            }
                        default:
                            break;
                    }
        }
    }

}

// Methods related to drawing to the screen

// Background is just a way to hold a temporary frame buffer, don't worry about it!
// set to the size of your biggest sprite (w*h) (could change in future)
uint16_t background[(60*60)];

// all parameters are in pixels
void Fill(int32_t x, int32_t y, int32_t xsize, int32_t ysize){
    for(int i = 0; i<ysize; i++){
        for(int j = 0; j<xsize; j++){
            background[xsize*i+j] = space[(159-y+i)*128+(x+j)];
        }
    }
}

// x position of sprite, y position of sprite, pointer to sprite image, width of sprite, height of sprite (in pixels)
void DrawOverSpace(int32_t x, int32_t y, const uint16_t *image, int32_t w, int32_t h){
    Fill(x,y,w,h);
    for(int j=0; j<(w*h); j++){
        uint16_t pixel = image[j];
        if(pixel){
            background[j] = pixel;
        }
    }
    ST7735_DrawBitmap(x,y,background,w,h);
}

// use when morphing a sprite (teleport from left to right for example)
void EraseOverSpace(int32_t x, int32_t y, int32_t w, int32_t h){
    Fill(x,y,w,h);
    ST7735_DrawBitmap(x,y,background,w,h);
}

// draws all alive sprites
// Always draw in this order: enemies->->player->projectiles
void draw(void){
    //drawings for enemies that takes care of despawned enemies


    for(int i = 0; i<NUMENEMIES; i++){
        if(enemy[i].life > 1){
            int32_t xdiff = enemy[i].x-enemy[i].lastx;
            int32_t ydiff = enemy[i].y-enemy[i].lasty;
            if(ydiff<0) ydiff = -ydiff;
            if(xdiff<0) xdiff = -xdiff;

            if((xdiff > 2<<FIX) || (ydiff > 1<<FIX)){
                EraseOverSpace(enemy[i].lastx>>FIX, enemy[i].lasty>>FIX,
                                                      enemy[i].w, enemy[i].h);
            }
            DrawOverSpace(enemy[i].x>>FIX, enemy[i].y>>FIX,
                              enemy[i].image[enemy[i].color][0],
                              enemy[i].w, enemy[i].h);
        }
        else if(enemy[i].life == 1){
            EraseOverSpace(enemy[i].lastx>>FIX, enemy[i].lasty>>FIX,
                              enemy[i].w, enemy[i].h);
            enemy[i].life = 0;
            if(enemy[i].type == 4){
                score += player.life*50;
                win = 1;
                end = 1;
                redrawbg = 1;
            }
        }

    }

    //laser drawings with same system as enemies
    for(int i = 0; i<NUMLASERS; i++){
            if(lasers[i].life == 1){
                EraseOverSpace(lasers[i].lastx>>FIX, lasers[i].lasty>>FIX,
                                                  lasers[i].w, lasers[i].h);

                DrawOverSpace(lasers[i].x>>FIX, lasers[i].y>>FIX,
                                                  lasers[i].image[lasers[i].color][0],
                                                  lasers[i].w, lasers[i].h);
            }
            else if(lasers[i].life == 2){
                  EraseOverSpace(lasers[i].lastx>>FIX, lasers[i].lasty>>FIX,
                                                    lasers[i].w, lasers[i].h);
                  lasers[i].life = 0;
            }

    }

    //missile drawings with the same system as enemies
    for(int i = 0; i<NUMMISSILES; i++){
            if(missiles[i].life == 1){
                int32_t xdiff = missiles[i].lastx - missiles[i].x;
                int32_t ydiff = missiles[i].lasty - missiles[i].y;
                if(xdiff<0) xdiff = -xdiff;
                if(ydiff<0) ydiff = -ydiff;
//                if((xdiff > 2<<FIX || ydiff > 2<<FIX) && missiles[i].type > 0){
//                    EraseOverSpace(missiles[i].lastx>>FIX, missiles[i].lasty>>FIX,
//                                  missiles[i].w, missiles[i].h);
//                }
//                else{
                    EraseOverSpace(missiles[i].lastx>>FIX, missiles[i].lasty>>FIX,
                                   missiles[i].w, missiles[i].h);
//                }
                DrawOverSpace(missiles[i].x>>FIX, missiles[i].y>>FIX,
                                                  missiles[i].image[missiles[i].color][0],
                                                  missiles[i].w, missiles[i].h);
            }
            else if(missiles[i].life == 2){
                  EraseOverSpace(missiles[i].lastx>>FIX, missiles[i].lasty>>FIX,
                                                    missiles[i].w, missiles[i].h);
                  missiles[i].life = 0;
            }
    }

    // draw the player with appropriate damage levels
    int32_t xdiff = player.lastx - player.x;
    int32_t ydiff = player.lasty - player.y;
    if(xdiff<0) xdiff = -xdiff;
    if(ydiff<0) ydiff = -ydiff;
    if(xdiff > 2<<FIX || ydiff){
        EraseOverSpace(player.lastx>>FIX, player.lasty>>FIX,
                                      player.w, player.h);
    }

    DrawOverSpace(player.x>>FIX, player.y>>FIX,
                              player.image[player.color][NUMHP-player.life],
                              player.w, player.h);

    SmallFont_OutVertical(score, 104, 6);
}

// games engine runs at 30Hz
void TIMG12_IRQHandler(void){uint32_t pos,msg;
  if((TIMG12->CPU_INT.IIDX) == 1){ // this will acknowledge
    GPIOB->DOUTTGL31_0 = GREEN; // toggle PB27 (minimally intrusive debugging)
// game engine goes here
    // 1) sample slide pot
    ADC_InDual(ADC1,&XData,&YData);

    // 2) read input switches
    lastshoot = shoots;
    shoots = Shoot_In();

    lastselects = selects;
    selects = Select_In();
    // optional hold down to shoot mode with shot limit
//    if(shoots == 1 && timer-lastshoot > 5){
//        lasers_init();
//        lastshoot = timer;
//    }
    if( (lastshoot == 0 && shoots == 1) || (lastups == 0 && ups == 1) && !title){
        lasers_init();
        Sound_Shoot();
    }


    lastups = ups;
    ups = Up_In();
    if((lastups == 0 && ups == 1) && title){
        Language = (Language+1)&(0x1);
        redrawbg = 1;
    }
//    lastups = ups;

    lastswaps = swaps;
    swaps = Swap_In();
    if((lastswaps == 0 && swaps == 1) || (lastselects == 0 && selects == 1) && !title){
        changecolor();
        Sound_Highpitch();
    }
//    lastswaps = swaps;

    // basically if click and some condition about how many shots the player has absorbed, set all enemy lives to 0 EXCEPT boss types if we have them
    click = JoyStick_InButton();

    // 3) move sprites and handle collisions if we're out of the title screen. Multiple moves will be needed for different enemy groups.
    // Check for collisions, incrementing score as needed or charging special attack
    // Getting hit should subtract from health and score, also stopping a potential streak feature
    // Hitting something should add to a streak
    if(!title){
        move();
        // represent player status with onboard LEDs
        ledstatus();

        // 4) start sounds (not needed)

        // 5) increment in game clock
        timer++;

        // after about nine seconds, spawn a wave
//        if(first){
//            spawnshiftenemy(60<<FIX,60<<FIX,0,3,1,0, 1);        //reverse direction
//            spawnshiftenemy(60<<FIX,50<<FIX,0,3,1,1, 2);        //normal direction(left), but quick movement
//            spawnshiftenemy(60<<FIX,80<<FIX,0,3,1,0, 1);
//            spawnshiftenemy(60<<FIX,70<<FIX,0,3,1,1, 2);
//            spawnshiftenemy(60<<FIX,100<<FIX,0,3,1,0, 1);        //reverse direction
//            spawnshiftenemy(60<<FIX,90<<FIX,0,3,1,1, 2);        //normal direction(left), but quick movement
//            spawnshiftenemy(60<<FIX,120<<FIX,0,3,1,0, 1);
//            spawnshiftenemy(60<<FIX,110<<FIX,0,3,1,1, 2);
//            spawnmediumenemy(56<<FIX,9<<FIX,0,0,3,1);
//            first = 0;
//        }
        if(timer%120 == 1){
            bosscolor++;
            if(bosscolor > 1){
                bosscolor = 0;
            }
        }

        // waves are every 6 seconds. We need 20 total waves
        if(first || timer%180 == 1){
            switch(wave)
            {
                case 0:
                    spawnsmallenemy(56<<FIX,30<<FIX,0,1,1,0,0);
                    first = 0;
                    wave++;
                    break;
                case 1:
                    Sound_Fastinvader1();
                    spawnline(YELLOWWAVE);
                    wave++;
                    break;
                case 2:
                    Sound_Fastinvader2();
                    spawnline(GREENWAVE);
                    wave++;
                    break;
                case 3:
                    wave++;
                    break;
                case 4:
                    Sound_Fastinvader2();
                    spawnbird();
                    spawnmediumenemy(56<<FIX,9<<FIX,0,0,3,1);
                    wave++;
                    break;
                case 5:
                    spawnx(GREENWAVE);
                    wave++;
                    break;
                case 6:
                    wave++;
                    break;
                case 7:
                    Sound_Fastinvader3();
                    spawnshiftenemy(20<<FIX,80<<FIX,0,1,1,1,0);
                    spawnshiftenemy(40<<FIX,60<<FIX,0,1,1,0,0);
                    spawnshiftenemy(60<<FIX,40<<FIX,0,1,1,1,0);
                    spawnshiftenemy(80<<FIX,60<<FIX,0,1,1,0,0);
                    spawnshiftenemy(100<<FIX,80<<FIX,0,1,1,1,0);
                    wave++;
                    break;
                case 8:
                    wave++;
                    break;
                case 9:
                    Sound_Fastinvader2();
                    spawnfastline(1, 25<<FIX);
                    spawnfastline(0, 40<<FIX);
                    wave++;
                    break;
                case 10:
                   Sound_Fastinvader2();
                   spawnshiftenemy(60<<FIX,30<<FIX,0,3,1,0, 1);        //reverse direction
                   spawnshiftenemy(60<<FIX,20<<FIX,0,3,1,1, 2);        //normal direction(left), but quick movement
                   spawnshiftenemy(60<<FIX,50<<FIX,0,3,1,0, 1);
                   spawnshiftenemy(60<<FIX,40<<FIX,0,3,1,1, 2);
                   spawnshiftenemy(60<<FIX,70<<FIX,0,3,1,0, 1);        //reverse direction
                   spawnshiftenemy(60<<FIX,60<<FIX,0,3,1,1, 2);        //normal direction(left), but quick movement
                   spawnshiftenemy(60<<FIX,90<<FIX,0,3,1,0, 1);
                   spawnshiftenemy(60<<FIX,80<<FIX,0,3,1,1, 2);
                   spawnmediumenemy(56<<FIX,9<<FIX,0,0,3,0);
                   wave++;
                   break;
                case 11:
                    wave++;
                    break;
                case 12:
                    spawnmediumenemy(16<<FIX,9<<FIX,0,2,1,0);
                    spawnmediumenemy(96<<FIX,9<<FIX,0,2,1,1);
                    spawnshiftenemy(20<<FIX,100<<FIX,0,1,1,1,0);
                    spawnshiftenemy(40<<FIX,80<<FIX,0,1,1,0,0);
                    spawnshiftenemy(60<<FIX,60<<FIX,0,1,1,1,0);
                    spawnshiftenemy(80<<FIX,80<<FIX,0,1,1,0,0);
                    spawnshiftenemy(100<<FIX,100<<FIX,0,1,1,1,0);
                    wave++;
                    break;
                case 13:
                    wave++;
                    break;
                case 14:
                    Sound_Fastinvader4();
                    spawnboss(40<<FIX, 30<<FIX, 0, 0, 10, 1);
                    spawnfastline(GREENWAVE, 35<<FIX);
                    spawnsmallenemy(1<<FIX,9<<FIX,0,0,1,0,0);
                    wave++;
                    break;
                default:
                    break;
            }
        }
    }
    else{
        if(lastselects == 0 && selects == 1){
            if(tutorial){
                title = 0;
            }
            else{
                tutorial = 1;
            }

            redrawbg = 1;
        }
        titletimer++;
    }

    // 6) set semaphore
    Flag = 1;

    // NO LCD OUTPUT IN INTERRUPT SERVICE ROUTINES
    GPIOB->DOUTTGL31_0 = GREEN; // toggle PB27 (minimally intrusive debugging)
  }
}
uint8_t TExaS_LaunchPadLogicPB27PB26(void){
  return (0x80|((GPIOB->DOUT31_0>>26)&0x03));
}

typedef enum {English, Spanish, Portuguese, French} Language_t;
Language_t myLanguage=English;
typedef enum {HELLO, GOODBYE, LANGUAGE} phrase_t;
const char Hello_English[] ="Hello";
const char Hello_Spanish[] ="\xADHola!";
const char Hello_Portuguese[] = "Ol\xA0";
const char Hello_French[] ="All\x83";
const char Goodbye_English[]="Goodbye";
const char Goodbye_Spanish[]="Adi\xA2s";
const char Goodbye_Portuguese[] = "Tchau";
const char Goodbye_French[] = "Au revoir";
const char Language_English[]="English";
const char Language_Spanish[]="Espa\xA4ol";
const char Language_Portuguese[]="Portugu\x88s";
const char Language_French[]="Fran\x87" "ais";


// TITLE SCREEN MESSAGES

const char Title_English[] = "Switch Force";
const char Title_Spanish[] = "Fuerza de Cambio";

const char Start_English[] = "Push right to start";
const char Start_Spanish[] = "Pulsa el bot\xA2n    ";
const char Start2_English[] = "               ";
const char Start2_Spanish[] = "derecho para jugar";

const char Select_English[] = "Para Espa\xA4ol, pulsa";
const char Select_Spanish[] = "For English, press";
const char Language2_English[] = "el bot\xA2n arriba    ";
const char Language2_Spanish[] = "up                ";


const char *Title[2] = {Title_English, Title_Spanish};
const char *Select[2] = {Select_English, Select_Spanish};
const char *Language2[2] = {Language2_English, Language2_Spanish};
//const char *Language3[2] = {Language3_English, Language3_Spanish};
const char *Start[2] = {Start_English, Start_Spanish};
const char *Start2[2] = {Start2_English, Start2_Spanish};

// TUTORIAL MESSAGES
const char Tutorial_English[] = "Tutorial";
const char Tutorial_Spanish[] = "Tutorial"; // convenient!

const char Fly_English[] = "Fly";
const char Fly_Spanish[] = "Volar";

const char Shoot_English[] = "Shoot";
const char Shoot_Spanish[] = "Disparar";

const char Switch_English[] = "Swap color";
const char Switch_Spanish[] = "Cambia color";

const char Mechanics1_English[] = "Match color to be";
const char Mechanics1_Spanish[] = "Combina color para";

const char Mechanics2_English[] = "immune and attack  ";
const char Mechanics2_Spanish[] = "ser inmune y atacar";

const char *Tutorial[2] = {Tutorial_English, Tutorial_Spanish};
const char *Fly[2] = {Fly_English, Fly_Spanish};
const char *Shoot[2] = {Shoot_English, Shoot_Spanish};
const char *Switch[2] = {Switch_English, Switch_Spanish};
const char *Mechanics1[2] = {Mechanics1_English, Mechanics1_Spanish};
const char *Mechanics2[2] = {Mechanics2_English, Mechanics2_Spanish};

// GAME OVER SCREEN MESSAGES

const char GameOver_English[] = "Game Over";
const char GameOver_Spanish[] = "Fin del Juego";

const char Status_English[] = "Status: ";
const char Status_Spanish[] = "Estatus: ";
const char StatusBad1_English[] = "Missing in";
const char StatusBad1_Spanish[] = "Falta en";
const char StatusBad2_English[] = "Action";
const char StatusBad2_Spanish[] = "Acci\xA2n";
const char StatusBad3_English[] = "";
const char StatusBad3_Spanish[] = "";


const char StatusGood1_English[] = "Mission is";
const char StatusGood1_Spanish[] = "Misi\xA2n";
const char StatusGood2_English[] = "complete, awaiting";
const char StatusGood2_Spanish[] = "completada";
const char StatusGood3_English[] = "reassignment";
const char StatusGood3_Spanish[] = "";

const char Score_English[] = "Score:";
const char Score_Spanish[] = "Marcador:";

const char *GameOver[2] = {GameOver_English, GameOver_Spanish};

const char *Status0[2] = {Status_English, Status_Spanish};
const char *Status1[2][2] = {
                             {StatusBad1_English, StatusBad1_Spanish},
                             {StatusGood1_English, StatusGood1_Spanish}
};
const char *Status2[2][2] = {
                             {StatusBad2_English, StatusBad2_Spanish},
                             {StatusGood2_English, StatusGood2_Spanish}
};

const char *Status3[2][2] = {
                             {StatusBad3_English, StatusBad3_Spanish},
                             {StatusGood3_English, StatusGood3_Spanish}
};


const char *Score[2] = {Score_English, Score_Spanish};


const char *Phrases[3][4]={
  {Hello_English,Hello_Spanish,Hello_Portuguese,Hello_French},
  {Goodbye_English,Goodbye_Spanish,Goodbye_Portuguese,Goodbye_French},
  {Language_English,Language_Spanish,Language_Portuguese,Language_French}
};


// use main1 to observe special characters
int main1(void){ // main1
    char l;
  __disable_irq();
  PLL_Init(); // set bus speed
  LaunchPad_Init();
  ST7735_InitPrintf();
  ST7735_FillScreen(0x0000);            // set screen to black
  for(phrase_t myPhrase=HELLO; myPhrase<= GOODBYE; myPhrase++){
    for(Language_t myL=English; myL<= French; myL++){
         ST7735_OutString((char *)Phrases[LANGUAGE][myL]);
      ST7735_OutChar(' ');
         ST7735_OutString((char *)Phrases[myPhrase][myL]);
      ST7735_OutChar(13);
    }
  }
  Clock_Delay1ms(3000);
  ST7735_FillScreen(0x0000);       // set screen to black
  l = 128;
  while(1){
    Clock_Delay1ms(2000);
    for(int j=0; j < 3; j++){
      for(int i=0;i<16;i++){
        ST7735_SetCursor(7*j+0,i);
        ST7735_OutUDec(l);
        ST7735_OutChar(' ');
        ST7735_OutChar(' ');
        ST7735_SetCursor(7*j+4,i);
        ST7735_OutChar(l);
        l++;
      }
    }
  }
}

// use main2 to observe graphics
int main2(void){ // main2
  __disable_irq();
  PLL_Init(); // set bus speed
  LaunchPad_Init();
  ST7735_InitPrintf();
    //note: if you colors are weird, see different options for
    // ST7735_InitR(INITR_REDTAB); inside ST7735_InitPrintf()
  ST7735_FillScreen(ST7735_BLACK);
  ST7735_DrawBitmap(22, 159, PlayerShip0, 18,8); // player ship bottom
  ST7735_DrawBitmap(53, 151, Bunker0, 18,5);
  ST7735_DrawBitmap(42, 159, PlayerShip1, 18,8); // player ship bottom
  ST7735_DrawBitmap(62, 159, PlayerShip2, 18,8); // player ship bottom
  ST7735_DrawBitmap(82, 159, PlayerShip3, 18,8); // player ship bottom
  ST7735_DrawBitmap(0, 9, SmallEnemy10pointA, 16,10);
  ST7735_DrawBitmap(20,9, SmallEnemy10pointB, 16,10);
  ST7735_DrawBitmap(40, 9, SmallEnemy20pointA, 16,10);
  ST7735_DrawBitmap(60, 9, SmallEnemy20pointB, 16,10);
  ST7735_DrawBitmap(80, 9, SmallEnemy30pointA, 16,10);

//  for(uint32_t t=500;t>0;t=t-5){
//    SmallFont_OutVertical(t,104,6); // top left
//    Clock_Delay1ms(50);              // delay 50 msec
//  }

  // Demo with enemy moving down at 1 pixel per frame
  for(uint32_t t=9;t<127;t++){
    ST7735_DrawBitmap(60, t, SmallEnemy10pointA, 16,10);
    SmallFont_OutVertical(t,104,6); // top left
    Clock_Delay1ms(33);              // delay 50 msec
  }

  ST7735_FillScreen(0x0000);   // set screen to black
  ST7735_SetCursor(1, 1);
  ST7735_OutString("GAME OVER");
  ST7735_SetCursor(1, 2);
  ST7735_OutString("Nice try,");
  ST7735_SetCursor(1, 3);
  ST7735_OutString("Earthling!");
  ST7735_SetCursor(2, 4);
  ST7735_OutUDec(1234);
  while(1){
  }
}

// use main3 to test switches and LEDs
int main3(void){ // main3
  __disable_irq();
  PLL_Init(); // set bus speed
  LaunchPad_Init();
  Switch_Init(); // initialize switches
  JoyStick_Init(); // initialize joystick (including the click stick)
  LED_Init(); // initialize LED
  while(1){
    // write code to test switches and LEDs
    uint32_t data = Shoot_In();
    if(data == 0) LED_Off(LEFT);
    if(data == 1) LED_On(LEFT);

    data = Select_In();
    if(data == 0) LED_Off(RIGHT);
    if(data == 1) LED_On(RIGHT);

    data = Swap_In();
    if(data == 0) LED_Off(MID);
    if(data == 1) LED_On(MID);

    data = JoyStick_InButton();
    if(data == 0) LED_On(MID);
    if(data == 1) LED_Off(MID);
  }
}
// use main4 to test sound outputs
int main4(void){ uint32_t last=0,now;
  __disable_irq();
  PLL_Init(); // set bus speed
  LaunchPad_Init();
  Switch_Init(); // initialize switches
  LED_Init(); // initialize LED
  Sound_Init();  // initialize sound
  TExaS_Init(ADC0,6,0); // ADC1 channel 6 is PB20, TExaS scope
  __enable_irq();
  while(1){
    now = Shoot_In(); // one of your buttons
    if((last == 0)&&(now == 1)){
      Sound_Shoot(); // call one of your sounds
    }
    if((last == 0)&&(now == 2)){
      Sound_Killed(); // call one of your sounds
    }
    if((last == 0)&&(now == 4)){
      Sound_Explosion(); // call one of your sounds
    }
    if((last == 0)&&(now == 8)){
      Sound_Fastinvader1(); // call one of your sounds
    }
    last = now;
    // modify this to test all your sounds
  }
}

// ALL ST7735 OUTPUT MUST OCCUR IN MAIN
int main(void){ // final main
  __disable_irq();
  PLL_Init(); // set bus speed
  LaunchPad_Init();
  //UART1_Init(); // just transmit, PA8, blind synchronization
  //UART2_Init(); // just receive, PA22, receiver timeout synchronization
  ST7735_InitPrintf();
    //note: if you colors are weird, see different options for
    // ST7735_InitR(INITR_REDTAB); inside ST7735_InitPrintf()
  //ST7735_FillScreen(ST7735_BLACK);

//  ST7735_DrawBitmap(30, 110, titleptr, 60, 60);         //loading screen
  ST7735_SetCursor(1, 6);
  ST7735_OutString("Developed by Aidan");
  ST7735_SetCursor(1, 7);
  ST7735_OutString("Aalund and Jason");
  ST7735_SetCursor(1, 8);
  ST7735_OutString("Ochiam");
  Clock_Delay1ms(2500);

  ST7735_DrawBitmap(0, 160, spaceptr, 128, 159);
  //ADCinit(); //PB18 = ADC1 channel 5, slidepot
  JoyStick_Init(); // Initialize stick
  Switch_Init(); // initialize switches
  LED_Init();    // initialize LED
  Sound_Init();  // initialize sound
  player_init();
  TExaS_Init(0,0,&TExaS_LaunchPadLogicPB27PB26); // PB27 and PB26 USE FOR DEBUGGING?
  // initialize interrupts on TimerG12 at 30 Hz
  TimerG12_IntArm(80000000/30,3); // low priority interrupt, lower than 3 is higher prio
  // initialize all data structures
  __enable_irq();
  while(1){
    while(Flag){
        if(title){
            if(redrawbg){
                ST7735_DrawBitmap(0, 159, spaceptr, 128, 160);
                if(!tutorial){
                    EraseOverSpace(35,80,60,60);
                    DrawOverSpace(35,80, titleptr, 60, 60);
//                    DrawOverSpace(10, 100, AlienBossABigYellow24, 48, 24);
                    DrawOverSpace(28, 95, SmallEnemy10pointGreenA, 16, 10);
                    DrawOverSpace(48, 95, SmallEnemy10pointYellowA, 16, 10);
                    DrawOverSpace(68, 95, SmallEnemy20pointGreenA, 16, 10);
                    DrawOverSpace(88, 95, SmallEnemy20pointYellowA, 16, 10);
                }
                redrawbg = 0;
                titleflag = 0;
            }

            if(titletimer % 60 == 1){
                titleflag++;
                if(titleflag > 1){
                    titleflag = 0;
                }
                LED_Toggle(LEFT);
                LED_Toggle(MID);
                LED_Toggle(RIGHT);
            }

            if(!tutorial){
                if(titleflag){
                    ST7735_SetCursor(1, 6+7);
                    ST7735_OutString((char *)Start[Language]);
                    ST7735_SetCursor(1, 7+7);
                    ST7735_OutString((char *)Start2[Language]);
                }
                else{
                    ST7735_SetCursor(1, 6+7);
                    ST7735_OutString((char *)Select[Language]);
                    ST7735_SetCursor(1, 7+7);
                    ST7735_OutString((char *)Language2[Language]);
                }
            }
            else{ // draw tutorial screen if tutorial flag is 1
                ST7735_SetCursor(7, 1);
                ST7735_OutString((char *)Tutorial[Language]);
                ST7735_SetCursor(1, 2);
                ST7735_OutString("STICK:");
                ST7735_OutString((char *)Fly[Language]);
                ST7735_SetCursor(1, 3);
                ST7735_OutString("LFT/RGHT:");
                ST7735_OutString((char *)Shoot[Language]);
                ST7735_SetCursor(1, 4);
                ST7735_OutString("DWN/UP:");
                ST7735_OutString((char *)Switch[Language]);
                ST7735_SetCursor(1, 6);
                ST7735_OutString((char *)Mechanics1[Language]);
                ST7735_SetCursor(1, 7);
                ST7735_OutString((char *)Mechanics2[Language]);

                //DrawOverSpace(int32_t x, int32_t y, const uint16_t *image, int32_t w, int32_t h)
                DrawOverSpace(10, 100, SmallEnemy10pointGreenA, 16, 10);
                DrawOverSpace(18, 113, LaserGreen0, 2, 9);
                DrawOverSpace(18, 124, LaserGreen0, 2, 9);
                DrawOverSpace(10, 139, PlayerShip0, 18, 8);
                // DRAW GREEN CHECK
                ST7735_Line(25, 145, 17, 155, 0x07E0);
                ST7735_Line(19, 155, 12, 150, 0x07E0);


                DrawOverSpace(55, 100, SmallEnemy10pointYellowA, 16, 10);
                DrawOverSpace(63, 113, LaserYellow0, 2, 9);
                DrawOverSpace(63, 124, LaserYellow0, 2, 9);
                DrawOverSpace(55, 139, PlayerShipYellow0, 18, 8);
                // DRAW GREEN CHECK
                ST7735_Line(57+12, 145, 49+12, 155, 0x07E0);
                ST7735_Line(51+12, 155, 44+12, 150, 0x07E0);

                DrawOverSpace(115-16, 100, SmallEnemy20pointYellowA, 16, 10);
                DrawOverSpace(115-16+8, 113, LaserGreen0, 2, 9);
                DrawOverSpace(115-16+8, 124, LaserGreen0, 2, 9);
                DrawOverSpace(115-16, 139, PlayerShip0, 18, 8);
                // DRAW RED X
                ST7735_Line(101, 145, 114, 155, 0x001F);
                ST7735_Line(101, 155, 114, 145, 0x001F);
            }

            Flag = 0;
        }
        // if we aren't on the title screen do other stuff
        else{
            if(redrawbg){
                if(!end){
                    ST7735_DrawBitmap(0, 159, spaceptr, 128, 160);
                    redrawbg = 0;
                }
                else{
                    ST7735_FillScreen(0x0000);   // set screen to black
                // draw a funny valvano thing
                  ST7735_SetCursor(6-(2*Language), 1);
                  if(win){
                      textcolor = (ST7735_GREEN);
                      valvanoptr = valvanohappy;
                      ST7735_DrawBitmap(44, 80, valvanoptr, 40, 52);
                  }
                  else{
                      textcolor = (ST7735_RED);
                      ST7735_DrawBitmap(44, 80, valvanoptr, 40, 50);
                  }


                  ST7735_DrawString(6-(2*Language),1,(char *)GameOver[Language],textcolor);
                  ST7735_SetCursor(1, 9);
                  if(!Language){
                      if(win){
                          ST7735_OutString("Valvano is happy!");
                      }
                      else{
                          ST7735_OutString("Valvano is sad!");
                      }
                  }
                  else{
                      if(win){
                          ST7735_OutString("Profe est\xA0 contento");
                      }
                      else{
                          ST7735_OutString("Valvano est\xA0 triste");
                      }

                  }
                  ST7735_SetCursor(1, 11);
                  ST7735_OutString((char *)Status0[Language]);
                  ST7735_OutString((char *)Status1[win][Language]);
                  ST7735_SetCursor(1, 12);
                  ST7735_OutString((char *)Status2[win][Language]);
                  ST7735_SetCursor(1, 13);
                  ST7735_OutString((char *)Status3[win][Language]);
                  ST7735_SetCursor(1, 14);
                  ST7735_OutString((char *)Score[Language]);
                  ST7735_OutUDec(score);
                  redrawbg = 0;
                }
            }
            // update ST7735R with sprites
            if(!end){
                draw();
            }
            // clear semaphore
            Flag = 0;
            if(end){
                if(win){
                    LED_On(LEFT);
                    LED_Off(RIGHT);
                }
                else{
                    LED_On(RIGHT);
                    LED_Off(LEFT);
                }
                LED_Off(MID);
                  if(selects){
                    // reset a bunch of flags so we go back to the title screen and everything gets reset.
                    redrawbg = 1;
                    timer = 0;
                    titletimer = 0;
                    score = 0;
                    wave = 0;
                    first = 1;
                    titleflag = 0; // poorly named, but indicates to the title screen to show the second part of a phrase
                    bosscolor = 1;
                    bosskilled = 0;
                    shiftvelocity = 1;
                    Flag = 0; // Semaphore
                    title = 1; // flag to indicate if the game is on the title screen or tutorial.
                    tutorial = 0; // flag to indicate if the game in the tutorial
                    player_init();
                    end = 0;
                    win = 0;
                    LED_Off(LEFT);
                    LED_Off(MID);
                    LED_Off(RIGHT);
                    //'turn off' all enemies, lasers, and other things
                    for(int i = 0; i<NUMENEMIES; i++){
                        if(enemy[i].life){
                            enemy[i].life = 0;
                        }
                    }
                    for(int i = 0; i<NUMLASERS; i++){
                        if(lasers[i].life){
                            lasers[i].life = 0;
                        }
                    }
                    for(int i = 0; i<NUMMISSILES; i++){
                        if(missiles[i].life){
                            missiles[i].life = 0;
                        }
                    }
                  }
            }
        }
    }
  }
}
