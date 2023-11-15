/*
 * LED.c
 *
 *  Created on: Nov 5, 2023
 *      Author: Aidan Aalund
 */
#include <ti/devices/msp/msp.h>
#include "../inc/LaunchPad.h"
// LaunchPad.h defines all the indices into the PINCM table
// TODO: not really sure this is right.
#define LEFT 1<<24
#define MID 1<<27
#define RIGHT 1<<28

// initialize your LEDs
void LED_Init(void){
    // TODO: likely broken
    // from left to right: PA24, PA27, PA28
    // PINCM
    //   bit 25 is HiZ
    //   bit 20 is drive strength
    //   bit 18 is input enable control
    //   bit 17 is pull up control
    //   bit 16 is pull down control
    //   bit 7 is PC peripheral connected, enable transparent data flow
    //   bit 0 selects GPIO function
    IOMUX->SECCFG.PINCM[PA24INDEX] = 0x00100081;
    IOMUX->SECCFG.PINCM[PA27INDEX] = 0x00100081;
    IOMUX->SECCFG.PINCM[PA28INDEX] = 0x00100081;
    GPIOA->DOE31_0 |= LEFT|MID|RIGHT;
    // DOE31_0 Data output enable

}
// data specifies which LED to turn on
void LED_On(uint32_t data){
    // write this
    // use DOUTSET31_0 register so it does not interfere with other GPIO
    GPIOA->DOUTSET31_0 = data;
}

// data specifies which LED to turn off
void LED_Off(uint32_t data){
    // write this
    // use DOUTCLR31_0 register so it does not interfere with other GPIO
    GPIOA->DOUTCLR31_0 = data;
}

// data specifies which LED to toggle
void LED_Toggle(uint32_t data){
    // write this
    // use DOUTTGL31_0 register so it does not interfere with other GPIO
    GPIOA->DOUTTGL31_0 = data;
}

void mainled(void){
    // TODO: incomplete
    Clock_Init80MHz(0);
    LaunchPad_Init();
    while(1){
        LED_On(LEFT|MID|RIGHT);
        Clock_Delay(8000000); // breakpoint here to debug
        LED_Off(LEFT|MID|RIGHT);
        Clock_Delay(8000000);
    }
}
