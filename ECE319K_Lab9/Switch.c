/*
 * Switch.c
 *
 *  Created on: Nov 5, 2023
 *      Author: Aidan Aalund
 */
#include <ti/devices/msp/msp.h>
#include "../inc/LaunchPad.h"
// LaunchPad.h defines all the indices into the PINCM table
void Switch_Init(void){
    // initialize all as inputs in gpio
    IOMUX->SECCFG.PINCM[PA26INDEX] = 0x00040081; // UP
    IOMUX->SECCFG.PINCM[PA25INDEX] = 0x00040081; // DOWN
    IOMUX->SECCFG.PINCM[PA31INDEX] = 0x00040081; // LEFT
    IOMUX->SECCFG.PINCM[PA12INDEX] = 0x00040081; // RIGHT
    IOMUX->SECCFG.PINCM[PB24INDEX] = 0x00040081; // STICK CLICK
}
// return current state of switches
uint32_t Switch_In(void){
    // write this
    uint32_t input;
    // input is a 5 bit number representing all of the switches being pressed
    // STICK RIGHT LEFT DOWN UP
    input = ((GPIOA->DIN31_0&(1<<26))<<0)+((GPIOA->DIN31_0&(1<<25))<<1)+((GPIOA->DIN31_0&(1<<31))<<2)
            +((GPIOA->DIN31_0&(1<<12))<<3)+((GPIOB->DIN31_0&(1<<24))<<4);
    return input;
}

// basic test of this method
// use main to debug the three input switches
static int main1(void){ // main
  uint32_t last=0,now;
  Clock_Init80MHz(0);
  LaunchPad_Init();
  Switch_Init(); // your Lab 4 initialization
  UART_Init();
  __enable_irq(); // UART uses interrupts
  //UART_OutString("Lab 9, Fall 2023, Debug switches\n\r");
  while(1){
    now = Switch_In(); // Your Lab4 input
    if(now != last){ // change
      //UART_OutString("Switch= 0x"); UART_OutUHex(now); UART_OutString("\n\r");
      Clock_Delay(800000); // breakpoint here to debug
    }
    last = now;
    Clock_Delay(800000); // 10ms, to debounce switch
  }
}
