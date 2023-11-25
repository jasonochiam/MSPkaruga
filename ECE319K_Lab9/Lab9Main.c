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

// misc defines
#define NUMCOLORS 2
#define NUMHP 4

//Flags
uint32_t Flag = 0; // Semaphore
uint32_t Language; // 0 for english, 1 for spanish
uint32_t title = 1; // flag to indicate if the game has gone past the title screen.

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
uint32_t redrawbg; // tells the game to redraw the background (needed for title screen logic)
uint32_t timer; // holds the current time, wonderful. Units: 33.3ms
uint32_t score; // the score for the level. Units: points
uint32_t end; // tells the game to end or not (slightly different than win)
uint8_t win; // indicates if the player has won the game (1 yes, 0 not yet)

// Misc
const uint16_t *spaceptr = space; // pointer to space image




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
    uint32_t type; // used to determine enemies. 0 is basic enemy (no shots), 1 fires lasers straight down periodically, 2 fires lasers toward enemy. This number can be used as an index to select a sprite too.
    int32_t x,y; // positive lower left corner. Units: pixels>>FIX
    int32_t lastx,lasty; // used to avoid sprite flicker. Units: pixels>>FIX
    uint8_t enemyl;   //checks for laser time, 0 for player, 1 for enemy
    const uint16_t *redimage;
    const uint16_t *greenimage;
    const uint16_t *image[NUMCOLORS][NUMHP]; // a 2d array of images. X axis is damage level and Y axis is color
    const uint16_t *image2; // the default image for the sprite, in cases where I haven't implemented multiple damage images
    const uint16_t *blankimage; // the image to render over where the sprite was (needed for fast movement)
    const uint16_t *swapanimation; // points to swap animation frames for the player
    // TODO: more image pointers as needed for animation frames, other status values
    int32_t w,h; // the width and height of the sprite. Units: pixels
    int32_t vx,vy; // the velocity of the sprite. Units: pixels>>FIX per second
};

typedef struct sprite sprite_t;

#define NUMENEMIES 50
sprite_t enemy[NUMENEMIES];
#define NUMLASERS 30
sprite_t lasers[NUMLASERS];
#define NUMMISSILES 20
sprite_t missiles[NUMMISSILES];
sprite_t player;

// Enemy pattern, spawns two small enemies at the top of the screen. Not using in final game!
void enemy_init(void){
    for(int i = 0; i<2; i++){
        enemy[i].life = 2; // spawn an enemy at (n-1) hp
        enemy[i].x = (32+32*i)<<FIX;
        enemy[i].y = 10<<FIX;
        enemy[i].image2 = SmallEnemy10pointA;
        enemy[i].blankimage = SmallEnemy10pointAblank;
        enemy[i].vx = 0;
        enemy[i].vy = 1; // NOTE: this is 1<<FIX pixel per frame moving DOWN.
        enemy[i].w = 16;
        enemy[i].h = 10;
    }
}

// TODO: make this
// void enemylaser(sprite_t enemy)

void enemyball(sprite_t enemy){     //will initialize a new bullet specifically if called for enemy sprite
    static uint8_t i = 0;
    if(i < NUMLASERS){
        // TODO: fix this
        lasers[i].life = 1;     //1 for alive and moving, 2 for collision, 0 for despawned
        lasers[i].x = enemy.x;   //start at center of player
        lasers[i].y = enemy.y + (32<<FIX);
        lasers[i].color = enemy.color;
        lasers[i].image[0][0] = GreenBall;
        lasers[i].image[1][0] = YellowBall;
        lasers[i].blankimage = eBall;
        lasers[i].vx = 0;
        lasers[i].vy = 1<<FIX; // NOTE: -10 is 1 pixel per frame moving DOWN.
        lasers[i].w = 14;
        lasers[i].h = 14;
        lasers[i].enemyl = 1;
        // TODO: fix this bitmath approach for free performance
        //i = (i+1)&(NUMLASERS-1);
        // if there are more lasers then on screen limit, take over the last laser fired
        i++;
        if(i == NUMLASERS){
            i = 0;
        }
    }
}

void spawnsmallenemy(uint32_t x,uint32_t y, int32_t vx, int32_t vy, uint32_t hp, uint32_t color){
// search the enemy array for an un-spawned enemy
    for(int i = 0; i<NUMENEMIES; i++){
        // set the first one you find to all the input parameters
        if(enemy[i].life == 0){
            enemy[i].life = hp+1; // effective starting hp values
            enemy[i].color = color;
            enemy[i].x = x;
            enemy[i].y = y;
            enemy[i].image[0][0] = SmallEnemy10pointGreenA;
            enemy[i].image[1][0] = SmallEnemy10pointYellowA;
            enemy[i].image2 = SmallEnemy10pointA;
            enemy[i].blankimage = SmallEnemy10pointAblank;
            enemy[i].vx = vx;
            enemy[i].vy = vy;
            enemy[i].w = 16;
            enemy[i].h = 10;
            enemy[i].type = Random(10) & 3;
            enemyball(enemy[i]);
            break;
        }

    }
    // if it is full, do nothing
}

// initialize player
// WARNING: THE PLAYER HEALTH SYSTEM WORKS DIFFERENT THEN ENEMY SYSTEM. IM SORRY MY CODE IS BAD
// TODO: fix player hp system
void player_init(void){
    player.x = player.lastx = 64<<FIX;
    player.y = player.lasty = 159<<FIX;
    player.life = 0; // 0 is 3/3 hp, 1 is 2/3 hp, 2 is 1/3 hp, 3 is dying, 4 is dead (despawned)
    player.image[0][0] = PlayerShip0;
    player.image[0][1] = PlayerShip1;
    player.image[0][2] = PlayerShip3;
    player.image[0][3] = PlayerShip4;
    player.image[1][0] = PlayerShipYellow0;
    player.image[1][1] = PlayerShipYellow1;
    player.image[1][2] = PlayerShipYellow3;
    player.image[1][3] = PlayerShip4;
    // TODO: add a ship explosion graphic?
    // TODO: add swap animation frames if you have all the time in the world
    player.blankimage = PlayerShip4;
    player.w = 18;
    player.h = 8;
}

void lasers_init(void){     //will initialize a new bullet every time switch is pressed with a max of 20 bullets on screen at once
    static uint8_t i = 0;
    if(i < NUMLASERS){
        // TODO: fix this
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
        lasers[i].enemyl = 0;       //type = player bullets

        i++;
        if(i == NUMLASERS){
            i = 0;
        }
    }

}


// uses the player's life to update PCB LEDs
void ledstatus(void){
    switch(player.life){
    case 0:
        LED_On(LEFT);
        LED_Off(MID);
        LED_Off(RIGHT);
        break;
    case 1:
        LED_Off(LEFT);
        LED_On(MID);
        LED_Off(RIGHT);
        break;
    case 2:
        LED_Off(LEFT);
        LED_Off(MID);
        LED_On(RIGHT);
        break;
    case 3:
        LED_Off(LEFT);
        LED_Off(MID);
        LED_Off(RIGHT);
        break;
    default:
        break;
    }
}

void collisions(void);


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

    // Why did I make another set of variables?
    // Valvano's code assumes we want to get a unsigned value, however we need it to be signed.
    // I could've changed the methods, but each method relies on another and I didn't want to mess around with it.
    // If we are looking for extra speed, maybe I can go and change the drivers so we don't need these two variables.
    // Convert to signed int reflecting +/-
    // What should the maximum velocity be? Experiment with values.
    // Dead-zone has a radius of 100
    // At the center, the Y value like to hang from 2080 to 2090
    // For X, 2010 to 2020
    x = XData-2010; // makes middle close to zero, left is positive, right is negative
    y = YData-2080;
    // Deadzone handler: this may not be needed
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
    if(player.y > 150<<FIX){
        player.y = 150<<FIX;
    }
    if(player.y < 0){
        player.y = 0;
    }

    // move lasers
    for(int j = 0; j < NUMLASERS; j++){
        if(lasers[j].life == 1){
            if(lasers[j].y <= 0 || lasers[j].y >= 157<<FIX){        //if bullet is offscreen, despawn
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

    // move enemies, then run collision checks
    // TODO: what if multiple hit on the same enemy occur? I don't think this would break anything at the moment.
    for(int i = 0; i<NUMENEMIES; i++){
            if(enemy[i].life > 1){     //check for bullet collision here with a nested for loop comparing dimensions of each bullet active and each enemy
                if(enemy[i].y >= 157<<FIX){
                 // this is space invaders logic, enemies 'win' when they move to bottom
//                    enemy[i].life = 2;
                    enemy[i].life = 1;
                    end = 1;    //used to end game in main if aliens win
                    Sound_Killed();

                }
                else{       //else move enemies
                    enemy[i].lastx = enemy[i].x;
                    enemy[i].lasty = enemy[i].y;
                    enemy[i].x += enemy[i].vx;
                    enemy[i].y += enemy[i].vy;

                }


               for(int j = 0; j < NUMLASERS; j++){
                   if(lasers[j].life == 1){
                       // recall that the 'position' of a sprite is the top left corner
                       if(
                       ((lasers[j].x <= (enemy[i].x + (enemy[i].w<<FIX))) && (lasers[j].x >= (enemy[i].x)))
                               && ((lasers[j].y <= (enemy[i].y + (enemy[i].h<<FIX))) && (lasers[j].y >= (enemy[i].y)))

                      ){
                      // if collision occurred and color matched, kill the enemy. In all collisions despawn sprite.
                           if(lasers[j].color == enemy[i].color && lasers[j].enemyl == 0){
                               enemy[i].life--;
                               //TODO: dink sound?

                           }
                           else{
                               Sound_Explosion();
                           }
                           if(lasers[j].enemyl == 0){       //if laser type is player and laser hits enemy
                               lasers[j].life = 2;
                           }
                       }
                       if((player.x-lasers[j].x)*(player.x-lasers[j].x)+(player.y-lasers[j].y)*(player.y-lasers[j].y) <= (500<<FIX)){
                                              // checking for enemy bullet collision with player
                                              // TODO: add checking for both bullet type and color
                                              player.life++;
                                              lasers[j].life = 2;
                                              if(player.life == 3){
                                                  end = 1;
                                              }
                                              Sound_Explosion();
                                          }
                   }
               }

               // TODO: square distance approximation doesn't feel right just yet, but it works more or less
               if( (player.x-enemy[i].x)*(player.x-enemy[i].x)+(player.y-enemy[i].y)*(player.y-enemy[i].y) <= (500<<FIX)){
                   // get rid of line below when i-frames are added
                   enemy[i].life = 1;
                   player.life++;
                   if(player.life == 3){
                       end = 1;
                   }
                   Sound_Explosion();
               }
               // TODO: invincibility frames after hit? Right now it just kills the enemy which can result in abuse


            }
    }
}

// Methods related to drawing to the screen

// QUESTION: what is background? a 16 bit array of length?
// Background is just a way to hold a temporary frame buffer, don't worry about it!
// set to the size of your biggest sprite (w*h) (could change in future)
uint16_t background[500];


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
    //Fill(x,y,w,h);
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

    // TODO: draw score with SmallFont_OutHorizontal from the headers



    for(int i = 0; i<NUMENEMIES; i++){
        if(enemy[i].life > 1){
            EraseOverSpace(enemy[i].lastx>>FIX, enemy[i].lasty>>FIX,
                                          enemy[i].w, enemy[i].h);
            DrawOverSpace(enemy[i].x>>FIX, enemy[i].y>>FIX,
                              enemy[i].image[enemy[i].color][0],
                              enemy[i].w, enemy[i].h);
        }
        else if(enemy[i].life == 1){
            EraseOverSpace(enemy[i].lastx>>FIX, enemy[i].lasty>>FIX,
                              enemy[i].w, enemy[i].h);
            enemy[i].life = 0;
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

    // draw the player with appropriate damage levels
    // TODO: flicker, this one will be tough to solve
    EraseOverSpace(player.lastx>>FIX, player.lasty>>FIX,
                              player.w, player.h);
    DrawOverSpace(player.x>>FIX, player.y>>FIX,
                                              player.image[player.color][player.life],
                                              player.w, player.h);

}


// games  engine runs at 30Hz
void TIMG12_IRQHandler(void){uint32_t pos,msg;
  if((TIMG12->CPU_INT.IIDX) == 1){ // this will acknowledge
    GPIOB->DOUTTGL31_0 = GREEN; // toggle PB27 (minimally intrusive debugging)
// game engine goes here
    // 1) sample slide pot
    ADC_InDual(ADC1,&XData,&YData);

    // 2) read input switches
    shoots = Shoot_In();

    // optional hold down to shoot mode with shot limit
//    if(shoots == 1 && timer-lastshoot > 5){
//        lasers_init();
//        lastshoot = timer;
//    }
    if(lastshoot == 0 && shoots == 1){
        lasers_init();
    }
    lastshoot = shoots;

    selects = Select_In();
    lastselects = selects;

    ups = Up_In();
    if(lastups == 0 && ups == 1 && title){
        Language = (Language+1)&(0x1);
        redrawbg = 1;
    }
    lastups = ups;


    // TODO: technically the player can swap color on title screen with the way this works right now. Not too much of an issue but just note it.
    swaps = Swap_In();
    if((lastswaps == 0 && swaps == 1) || (lastups == 0 && ups == 1)){
        changecolor();
    }
    lastswaps = swaps;

    // TODO: figure out screen clear move (if at all)
    // basically if click and some condition about how many shots the player has absorbed, set all enemy lives to 0 EXCEPT boss types if we have them
    click = JoyStick_InButton();

    // 3) move sprites and handle collisions if we're out of the title screen. Multiple moves will be needed for different enemy groups.
    // Check for collisions, incrementing score as needed or charging special attack
    // Getting hit should subtract from health and score, also stopping a potential streak feature
    // Hitting something should add to a streak
    // TODO: add color based collisions
    if(!title){
        move();
        // represent player status with onboard LEDs
        ledstatus();

        // 4) start sounds (not needed)

        // 5) increment in game clock
        timer++;
        if(timer%60 == 0){
            spawnsmallenemy(32<<FIX,10<<FIX,0,1<<FIX,1,0);
            spawnsmallenemy(64<<FIX,10<<FIX,0,1<<FIX,1,1);
        }
    }
    else{
        if(selects){
            title = 0;
            redrawbg = 1;
        }
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

const char Start_English[] = "Press the right";
const char Start_Spanish[] = "Pulsa el bot\xA2n";
const char Start2_English[] = "button to start";
const char Start2_Spanish[] = "derecho para jugar";

const char Select_English[] = "Para Espa\xA4ol, pulsa";
const char Select_Spanish[] = "For English, press";
const char Language2_English[] = "el bot\xA2n arriba";
const char Language2_Spanish[] = "up";


const char *Title[2] = {Title_English, Title_Spanish};
const char *Select[2] = {Select_English, Select_Spanish};
const char *Language2[2] = {Language2_English, Language2_Spanish};
//const char *Language3[2] = {Language3_English, Language3_Spanish};
const char *Start[2] = {Start_English, Start_Spanish};
const char *Start2[2] = {Start2_English, Start2_Spanish};

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
  //TODO: title screen logic goes here. This will allow us to avoid drawing the bg every frame.



  ST7735_DrawBitmap(0, 160, spaceptr, 128, 159);
  //ADCinit();     //PB18 = ADC1 channel 5, slidepot
  JoyStick_Init(); // Initialize stick
  Switch_Init(); // initialize switches
  LED_Init();    // initialize LED
  Sound_Init();  // initialize sound
//  enemy_init(); for testing
  player_init();
  TExaS_Init(0,0,&TExaS_LaunchPadLogicPB27PB26); // PB27 and PB26 USE FOR DEBUGGING?
  // initialize interrupts on TimerG12 at 30 Hz
  TimerG12_IntArm(80000000/30,3); // low priority interrupt, lower than 3 is higher prio
  // initialize all data structures
  __enable_irq();
  while(1){
    while(Flag){
        if(title){
            // TODO: title screen logo. If you make anything from scratch let it be this
            if(redrawbg){
                ST7735_DrawBitmap(0, 160, spaceptr, 128, 159);
                redrawbg = 0;
            }
            ST7735_SetCursor(5-(2*Language), 1);
            ST7735_OutString((char *)Title[Language]);

            ST7735_SetCursor(1, 3);
            ST7735_OutString((char *)Start[Language]);
            ST7735_SetCursor(1, 4);
            ST7735_OutString((char *)Start2[Language]);

            ST7735_SetCursor(1, 6);
            ST7735_OutString((char *)Select[Language]);
            ST7735_SetCursor(1, 7);
            ST7735_OutString((char *)Language2[Language]);
            ST7735_SetCursor(1, 8);
            Flag = 0;
        }
        else{
            if(redrawbg){
                ST7735_DrawBitmap(0, 160, spaceptr, 128, 159);
                redrawbg = 0;
            }
            // update ST7735R with sprites
            draw();
            // clear semaphore
            Flag = 0;
        }
    }

    if(end){
        TIMG12->CPU_INT.IMASK = 0; // zero event mask
        LED_Off(LEFT);
        LED_Off(MID);
        LED_On(RIGHT);
        ST7735_FillScreen(0x0000);   // set screen to black
          ST7735_SetCursor(6-(2*Language), 1);
          ST7735_OutString((char *)GameOver[Language]);
          ST7735_SetCursor(1, 3);
          ST7735_OutString((char *)Status0[Language]);
          ST7735_OutString((char *)Status1[win][Language]);
          ST7735_SetCursor(1, 4);
          ST7735_OutString((char *)Status2[win][Language]);
          ST7735_SetCursor(1, 5);
          ST7735_OutString((char *)Status3[win][Language]);
          ST7735_SetCursor(1, 7);
          ST7735_OutString((char *)Score[Language]);
          ST7735_OutUDec(1234);

          while(1){
          }
    }

  }
}
