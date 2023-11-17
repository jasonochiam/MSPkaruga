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
}
// return current state of switches
//uint32_t Switch_In(void){
//    // write this
//    uint32_t input;
//    // input is a 5 bit number representing all of the switches being pressed
//    // STICK RIGHT LEFT DOWN UP
//    input = ((GPIOA->DIN31_0&(1<<26))>>26)+((GPIOA->DIN31_0&(1<<25))>>24)+((GPIOA->DIN31_0&(1<<31))>>29)
//            +((GPIOA->DIN31_0&(1<<12))>>9)+((GPIOB->DIN31_0&(1<<24))>>20);
//    return input;
//}

// return the state of the shoot switch (left)
uint32_t Shoot_In(void){
    uint32_t input;
    input = (GPIOA->DIN31_0&(1<<31))>>31;
    return input;
}

// return state of select switch (right)
uint32_t Select_In(void){
    uint32_t input;
    input = (GPIOA->DIN31_0&(1<<12))>>12;
    return input;
}

// return state of swap switch (bottom)
uint32_t Swap_In(void){
    uint32_t input;
    input = (GPIOA->DIN31_0&(1<<25))>>25;
    return input;
}

// return state of up switch (up)
uint32_t Up_In(void){
    uint32_t input;
    input = (GPIOA->DIN31_0&(1<<26))>>26;
    return input;
}


// use main 3 to debug the switches
