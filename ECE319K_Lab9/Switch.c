/*
 * Switch.c
 *
 *  Created on: Nov 5, 2023
 *      Author:
 */
#include <ti/devices/msp/msp.h>
#include "../inc/LaunchPad.h"
// LaunchPad.h defines all the indices into the PINCM table
void Switch_Init(void){
    // write this
    // TODO: change the indexes to reflect our switches
    IOMUX->SECCFG.PINCM[PB0INDEX] = 0x00100081;
    IOMUX->SECCFG.PINCM[PB1INDEX] = 0x00100081;
    IOMUX->SECCFG.PINCM[PB2INDEX] = 0x00100081;
    IOMUX->SECCFG.PINCM[PB6INDEX] = 0x00100081;
    IOMUX->SECCFG.PINCM[PB7INDEX] = 0x00100081;
}
// return current state of switches
uint32_t Switch_In(void){
    // write this

  return GPIOA->DIN31_0; //replace this your code
}

// TODO: write a version of Switch_In that masks the 5 pins we use as switches
