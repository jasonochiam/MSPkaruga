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
#include "images/images.h"


// Random defines go here
// LED defines
#define LEFT 1<<24
#define MID 1<<27
#define RIGHT 1<<28
#define FIX 4

uint32_t Flag = 0; // Semaphore
uint32_t Language; // 0 for english, 1 para espa�ol
uint32_t XData; // Variable that holds X position of stick
uint32_t YData; // Variable that holds Y position of stick
uint32_t shoots;
uint32_t lastshoot;
uint32_t selects;
uint32_t swaps;
uint32_t ups;
uint32_t click;


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
    const uint16_t *blankimage; // the image to render over where the sprite was (needed for fast movement)
    // TODO: more image pointers as needed for animation frames, other status values
    int32_t w,h; // the width and height of the sprite
    int32_t vx,vy; // the velocity of the sprite
};

typedef struct sprite sprite_t;

// initialize sprites.
#define NUMENEMIES 50
sprite_t enemy[NUMENEMIES];
#define NUMLASERS 20
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

// moves all enemies
void move(void){
    uint32_t data = ADCin(); // 0 to 4095
    player.lastx = player.x;
    player.x = (data>>5)<<FIX; //0 to 127
    // TODO: implement joystick controls
    if(player.x > 110<<FIX){
        player.x = 110<<FIX;
    }
    for(int i = 0; i<NUMENEMIES; i++){
        if(enemy[i].life == 1){
            if(enemy[i].y >= 157<<FIX){
             // this is space invaders logic, enemies 'win' when they move to bottom
                enemy[i].life = 2;
               //Sound_Killed(); uncomment later, seems to break the game
            }
            else{
                enemy[i].x += enemy[i].vx;
                enemy[i].y += enemy[i].vy;
            }

        }
    }
}

// initialize player
void player_init(void){
    player.x = player.lastx = 64<<FIX;
    player.y = player.lasty = 159<<FIX;
    player.life = 1;
    player.image = PlayerShip0;
    // TODO: program ship damage indicators
    player.blankimage = PlayerShip4; // 4 represents a dead ship, states 1-3 are intermediate.
    player.w = 18;
    player.h = 8;
}

// draws all enemies, REMEMBER TO SHIFT RIGHT BY SIX
void draw(void){
    // TODO: this sprite anti-flicker code will probably break when we add stick controls. It might not even work now
    // draw blank over last player position
    if(player.lastx != player.x){
    ST7735_DrawBitmap(player.lastx>>FIX, player.lasty>>FIX,
                      player.blankimage,
                      player.w, player.h);
    }
    ST7735_DrawBitmap(player.x>>FIX, player.y>>FIX,
                      player.image,
                      player.w, player.h);

    for(int i = 0; i<NUMENEMIES; i++){
        if(enemy[i].life == 1){
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
}


// games  engine runs at 30Hz
void TIMG12_IRQHandler(void){uint32_t pos,msg;
  if((TIMG12->CPU_INT.IIDX) == 1){ // this will acknowledge
    GPIOB->DOUTTGL31_0 = GREEN; // toggle PB27 (minimally intrusive debugging)
    GPIOB->DOUTTGL31_0 = GREEN; // toggle PB27 (minimally intrusive debugging)
// game engine goes here
    // 1) sample slide pot
    ADC_InDual(ADC1,&XData,&YData);

    // 2) read input switches
    shoots = Shoot_In();
    // TODO: uncomment this section once a laser shoot has been added
    // Note: this if statement prevents the player from holding down shoot and firing. I think this is a good change.
    // Otherwise we have two options, allow shots to fire 30 times a second, or find another way to limit fire rate.
//    if(lastshoot == 0) &&(shoots == 1)){
//        laser_shoot(player.x+(9<<FIX),player.y-(8>>FIX),-16);
//    }
    lastshoot = shoots;
    selects = Select_In();
    if(selects){
        Language = (Language+1)&(0x1);
    }
    swaps = Swap_In();
    // TODO: player color swap logic here
    ups = Up_In();
    // TODO: figure out what up will do, if anything.
    click = JoyStick_InButton();
    // TODO: figure out screen clear move

    // 3) move sprites and handle collisions
    move();
    // TODO: check for collisions, incrementing score as needed or charging special attack
    // Getting hit should subtract from health and score, also stopping a potential streak feature
    // Hitting something should add to a streak
    // 4) start sounds
    // Jason said this wasn't needed, so I didn't write it.

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
  ADCinit();     //PB18 = ADC1 channel 5, slidepot
  //JoyStick_Init(); // Initialize stick
  Switch_Init(); // initialize switches
  LED_Init();    // initialize LED
  Sound_Init();  // initialize sound
  enemy_init();
  player_init();
  //TExaS_Init(0,0,&TExaS_LaunchPadLogicPB27PB26); // PB27 and PB26 USE FOR DEBUGGING?
  // initialize interrupts on TimerG12 at 30 Hz
  TimerG12_IntArm(80000000/30,3); // low priority interrupt, lower than 3 is higher prio
  // initialize all data structures
  __enable_irq();

  while(1){
    if(Flag){
       // update ST7735R
       draw();
       // clear semaphore
       Flag = 0;
       // TODO: check for end game or level switch
    }
  }
}
