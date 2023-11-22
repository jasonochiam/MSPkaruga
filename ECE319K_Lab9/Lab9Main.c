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

uint32_t Flag = 0; // Semaphore
uint32_t Language; // 0 for english, 1 para español
uint32_t XData; // Variable that holds X position of stick
uint32_t YData; // Variable that holds Y position of stick
int32_t x;
int32_t y;
uint32_t shoots;
uint32_t lastshoot;
uint32_t selects;
uint32_t swaps;
uint32_t ups;
uint32_t click;
uint8_t end = 0;




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
    uint32_t life; // 0 dead, 1 alive, 2 is dying
    uint32_t color; // 0 is blue, 1 is red (use this for checking bullet-player collisions AFTER you know a hit has occurred)
    int32_t x,y; // positive lower left corner
    int32_t lastx,lasty; // used to avoid sprite flicker
    uint32_t health;
    const uint16_t *image; // the default image for the sprite
    const uint16_t *twohp;
    const uint16_t *onehp;
    const uint16_t *blankimage; // the image to render over where the sprite was (needed for fast movement)
    // TODO: more image pointers as needed for animation frames, other status values
    int32_t w,h; // the width and height of the sprite
    int32_t vx,vy; // the velocity of the sprite
};

typedef struct sprite sprite_t;

// initialize sprites.
#define NUMENEMIES 50
sprite_t enemy[NUMENEMIES];
#define NUMLASERS 30
sprite_t lasers[NUMLASERS];
#define NUMMISSILES 20
sprite_t missiles[NUMMISSILES];
sprite_t player;

// Enemy pattern, spawns two small enemies at the top of the screen.
void enemy_init(void){
    for(int i = 0; i<2; i++){
        enemy[i].life = 1;
        enemy[i].x = (32+32*i)<<FIX;
        enemy[i].y = 10<<FIX;
        enemy[i].image = SmallEnemy10pointA;
        enemy[i].blankimage = SmallEnemy10pointAblank;
        enemy[i].vx = 0;
        enemy[i].vy = 1; // NOTE: this is 1 pixel per frame moving DOWN.
        enemy[i].w = 16;
        enemy[i].h = 10;
    }
}

// initialize player
void player_init(void){
    player.x = player.lastx = 64<<FIX;
    player.y = player.lasty = 159<<FIX;
    player.life = 1; // 1 is 3/3 hp, 2 is 2/3 hp, 3 is 1/3 hp, 4 is dying, 5 is dead (despawned)
    player.image = PlayerShip0;
    player.twohp = PlayerShip1;
    player.onehp = PlayerShip3;
    // TODO: add a ship explosion graphic
    player.blankimage = PlayerShip4;
    player.w = 18;
    player.h = 8;
}

//TODO: moving forward while shooting can cause lasers to disappear. Unsure of true reason why.
void lasers_init(void){     //will initialize a new bullet every time switch is pressed with a max of 20 bullets on screen at once
    static uint8_t i = 0;
    if(i < NUMLASERS){
        lasers[i].life = 1;     //1 for alive and moving, 2 for collision, 0 for despawned
        lasers[i].x = player.x+(player.w<<(FIX-1));   //start at center of player
        lasers[i].y = player.y;
        lasers[i].image = Laser0;
        lasers[i].blankimage = eLaser0;
        lasers[i].vx = 0;
        lasers[i].vy = -20; // NOTE: -10 is 1 pixel per frame moving DOWN.
        lasers[i].w = 2;
        lasers[i].h = 9;
        // Numlasers = 0x14 = 0001 0100
        // Jason, your bit math doesn't work here (results in max sprite limit of 4) but I like the approach of using the label.
        //I can't figure something out so I've done a slower approach
        // TODO: fix this bitmath approach for free performance
        //i = (i+1)&(NUMLASERS-1);
        // if there are more lasers then on screen limit, take over the last laser fired
        i++;
        if(i == NUMLASERS){
            i = 0;
        }
    }

}

// uses the player's life to update PCB LEDs
void ledstatus(void){
    switch(player.life){
    case 1:
        LED_On(LEFT);
        LED_Off(MID);
        LED_Off(RIGHT);
        break;
    case 2:
        LED_Off(LEFT);
        LED_On(MID);
        LED_Off(RIGHT);
        break;
    case 3:
        LED_Off(LEFT);
        LED_Off(MID);
        LED_On(RIGHT);
        break;
    case 4:
        LED_Off(LEFT);
        LED_Off(MID);
        LED_Off(RIGHT);
        break;
    default:
        break;
    }
}

void collisions(void);

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
            if(lasers[j].y <= 0){        //if bullet is offscreen, despawn
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
            if(enemy[i].life == 1){     //check for bullet collision here with a nested for loop comparing dimensions of each bullet active and each enemy
                if(enemy[i].y >= 157<<FIX){
                 // this is space invaders logic, enemies 'win' when they move to bottom
                    enemy[i].life = 2;
                    end = 1;    //used to end game in main if aliens win
                    Sound_Killed(); //uncomment later, seems to break the game

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
                      // if collision occurred, kill the enemy
                           enemy[i].life = 2;
                           lasers[j].life = 2;
                           Sound_Killed();
                       }
                   }
               }

               // TODO: square distance approximation doesn't feel right just yet, but it works more or less
               if( (player.x-enemy[i].x)*(player.x-enemy[i].x)+(player.y-enemy[i].y)*(player.y-enemy[i].y) <= (500<<FIX)){
                   enemy[i].life = 2;
                   //TODO: implement life system
                   player.life++;
                   // TODO: replace with hurt sound
                   Sound_Killed();
               }
               // TODO: invincibility frames after hit? Right now it just kills the enemy which can result in abuse.


               // represent player status with onboard LEDs
               ledstatus();


            }
    }
}

// TODO: run all collision checks in this method, it won't have a void parameter type when finished.
//void collisions(void){
//
//}

// draws all enemies, REMEMBER TO SHIFT RIGHT BY SIX
// Always draw in this order: enemies->player->projectiles
void draw(void){

    // TODO: add background image (in ROM)
    // TODO: implement background image code (copy frame buffer)
    //drawings for enemies that takes care of despawned enemies
    for(int i = 0; i<2; i++){
        if(enemy[i].life == 1){
            ST7735_DrawBitmap(enemy[i].lastx>>FIX, enemy[i].lasty>>FIX,
                                          enemy[i].blankimage,
                                          enemy[i].w, enemy[i].h);
            ST7735_DrawBitmap(enemy[i].x>>FIX, enemy[i].y>>FIX,
                              enemy[i].image,
                              enemy[i].w, enemy[i].h);
        }
        else if(enemy[i].life == 2){
            ST7735_DrawBitmap(enemy[i].x>>FIX, enemy[i].y>>FIX,
                              enemy[i].blankimage,
                              enemy[i].w, enemy[i].h);
                              enemy[i].life = 0;
        }

    }
    //laser drawings with same system as enemies
    for(int i = 0; i<NUMLASERS; i++){
            if(lasers[i].life == 1){
                ST7735_DrawBitmap(lasers[i].lastx>>FIX, lasers[i].lasty>>FIX,
                                  lasers[i].blankimage,
                                  lasers[i].w, lasers[i].h);
                ST7735_DrawBitmap(lasers[i].x>>FIX, lasers[i].y>>FIX,
                                  lasers[i].image,
                                  lasers[i].w, lasers[i].h);
            }
            else if(lasers[i].life == 2){
                // hacky fix to prevent leftover laser pixels (may break in future)
                ST7735_DrawBitmap(lasers[i].lastx>>FIX, lasers[i].lasty>>FIX,
                                  lasers[i].blankimage,
                                  lasers[i].w, lasers[i].h);
                                  lasers[i].life = 0;
            }

        }

    // draw the player with appropriate damage levels
    if((player.x>>FIX != player.lastx>>FIX) || (player.y>>FIX != player.lasty>>FIX)){
        ST7735_DrawBitmap(player.lastx>>FIX, player.lasty>>FIX,
                          player.blankimage,
                          player.w, player.h);
    }

    switch(player.life){
        case 1:
            ST7735_DrawBitmap(player.x>>FIX, player.y>>FIX,
                                  player.image,
                                  player.w, player.h);
            break;
        case 2:
            ST7735_DrawBitmap(player.x>>FIX, player.y>>FIX,
                                  player.twohp,
                                  player.w, player.h);
            break;
        case 3:
            ST7735_DrawBitmap(player.x>>FIX, player.y>>FIX,
                                  player.onehp,
                                  player.w, player.h);
            break;
        case 4:
            ST7735_DrawBitmap(player.x>>FIX, player.y>>FIX,
                                  player.blankimage,
                                  player.w, player.h);
            break;
        default:
            break;
        }



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

    if(lastshoot == 0 && shoots == 1){
        lasers_init();
    }

    lastshoot = shoots;
    selects = Select_In();
    if(selects){
        Language = (Language+1)&(0x1);
    }
    // TODO: player color swap logic here
    swaps = Swap_In();
    // TODO: figure out what up will do, if anything.
    ups = Up_In();
    // TODO: figure out screen clear move
    click = JoyStick_InButton();

    // 3) move sprites and handle collisions. Multiple moves will be needed for different enemy groups.
    // Check for collisions, incrementing score as needed or charging special attack
    // Getting hit should subtract from health and score, also stopping a potential streak feature
    // Hitting something should add to a streak
    // TODO: do player collisions with enemy attacks (when added)
    move();

    // 4) start sounds (not needed)

    // 5) set semaphore
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
  ST7735_FillScreen(ST7735_BLACK);
  //ADCinit();     //PB18 = ADC1 channel 5, slidepot
  JoyStick_Init(); // Initialize stick
  Switch_Init(); // initialize switches
  LED_Init();    // initialize LED
  Sound_Init();  // initialize sound
  enemy_init();
  player_init();
  TExaS_Init(0,0,&TExaS_LaunchPadLogicPB27PB26); // PB27 and PB26 USE FOR DEBUGGING?
  // initialize interrupts on TimerG12 at 30 Hz
  TimerG12_IntArm(80000000/30,3); // low priority interrupt, lower than 3 is higher prio
  // initialize all data structures
  __enable_irq();
  while(1){
    while(Flag){
       // update ST7735R
       draw();
       // clear semaphore
       Flag = 0;
       // TODO: check for end game or level switch
    }

    if(end){
        TIMG12->CPU_INT.IMASK = 0; // zero event mask
        ST7735_FillScreen(0x0000);   // set screen to black
          ST7735_SetCursor(1, 1);
          ST7735_OutString("GAME OVER");
          ST7735_SetCursor(1, 2);
          ST7735_OutString("Nice try,");
          ST7735_SetCursor(1, 3);
          ST7735_OutString("Earthling!");
          ST7735_SetCursor(1, 4);
          ST7735_OutUDec(1234);

          while(1){
          }
    }

  }
}
