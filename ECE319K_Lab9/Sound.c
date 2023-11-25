// Sound.c
// Runs on MSPM0
// Sound assets in sounds/sounds.h
// Jonathan Valvano
// 11/15/2021 
#include <stdint.h>
#include <ti/devices/msp/msp.h>
#include "Sound.h"
#include "sounds/sounds.h"
#include "DAC5.h"
#include "../inc/Timer.h"
#include "../inc/LaunchPad.h"


uint32_t SoundIndex;
uint8_t *ptsound;
uint32_t soundsize;

void SysTick_IntArm(uint32_t period, uint32_t priority){    //typical intarm
// write this
    SysTick-> CTRL = 0x0;
    SysTick-> LOAD = period-1;
    SCB->SHP[1]    = (SCB->SHP[1]&(~0xC0000000))|priority<<30;
    SysTick-> VAL = 0x0;
    SysTick-> CTRL = 0x07;
}
// initialize a 11kHz SysTick, however no sound should be started
// initialize any global variables
// Initialize the 5 bit DAC
void Sound_Init(void){      //arm systick and dac, and reset global
    SysTick_IntArm(1,0);
    DAC5_Init();
    SoundIndex = 0;
}
void SysTick_Handler(void){ // called at 11 kHz
  // output one value to DAC if a sound is active
    DAC5_Out(ptsound[SoundIndex]>>3);
    SoundIndex++;
    if(SoundIndex >= soundsize){        //stop playing once sound is done
        SysTick->LOAD = 0;
        GPIOB->DOUTCLR31_0 = RED; // toggle PB27 (minimally intrusive debugging)
    }

}

//******* Sound_Start ************
// This function does not output to the DAC. 
// Rather, it sets a pointer and counter, and then enables the SysTick interrupt.
// It starts the sound, and the SysTick ISR does the output
// feel free to change the parameters
// Sound should play once and stop
// Input: pt is a pointer to an array of DAC outputs
//        count is the length of the array
// Output: none
// special cases: as you wish to implement
void Sound_Start(){        //not needed, as its done in the individual sound functions
    //SysTick->LOAD = 80000000/11025-1;
}
void Sound_Shoot(void){
// write this
    SoundIndex = 0;     //reset the sound index
    ptsound = shoot;    //set pointer to sound file
    soundsize = 4080;   //set size so you know when to stop
    SysTick->LOAD = 80000000/11025-1;   //this is interrupting at 11khz
}
void Sound_Killed(void){
    SoundIndex = 0;
    ptsound = invaderkilled;
    soundsize = 3377;
    SysTick->LOAD = 80000000/11025-1;
}
void Sound_Explosion(void){
    SoundIndex = 0;
    ptsound = explosion;
    soundsize = 2000;
    SysTick->LOAD = 80000000/11025-1;
}

void Sound_Fastinvader1(void){
    SoundIndex = 0;
    ptsound = fastinvader1;
    soundsize = 982;
    SysTick->LOAD = 80000000/11025-1;
}
void Sound_Fastinvader2(void){
    SoundIndex = 0;
    ptsound = fastinvader2;
    soundsize = 1042;
    SysTick->LOAD = 80000000/11025-1;
}
void Sound_Fastinvader3(void){
    SoundIndex = 0;
    ptsound = fastinvader3;
    soundsize = 1054;
    SysTick->LOAD = 80000000/11025-1;
}
void Sound_Fastinvader4(void){
    SoundIndex = 0;
    ptsound = fastinvader4;
    soundsize = 1098;
    SysTick->LOAD = 80000000/11025-1;
}
void Sound_Highpitch(void){
    SoundIndex = 0;
    ptsound = highpitch;
    soundsize = 1802;
    SysTick->LOAD = 80000000/11025-1;
}
