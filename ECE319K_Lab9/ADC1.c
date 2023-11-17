/*
 * ADC1.c
 *
 *  Your lab 8 solution
 */
#include <stdio.h>
#include <stdint.h>
#include <ti/devices/msp/msp.h>
#include "../inc/Clock.h"

void ADCinit(void){
    // write code to initialize ADC1 channel 5, PB18
    // Your measurement will be connected to PB18
    // 12-bit mode, 0 to 3.3V, right justified
    // software trigger, no averaging
        ADC1->ULLMEM.GPRCM.RSTCTL = 0xB1000003; // reset
        ADC1->ULLMEM.GPRCM.PWREN = 0x26000001; // turn on
        Clock_Delay(24); // blind wait for stability
        ADC1->ULLMEM.GPRCM.CLKCFG = 0xA9000000; // 4) ULPCLK
        ADC1->ULLMEM.CLKFREQ = 7; // 5) 40-48 MHz
        ADC1->ULLMEM.CTL0 = 0x03010000; // 6) divide by 8
        ADC1->ULLMEM.CTL1 = 0x00000000; // 7) mode
        ADC1->ULLMEM.CTL2 = 0x00000000; // 8) MEMRES
        ADC1->ULLMEM.MEMCTL[0] = 5; // 9) channel
        ADC1->ULLMEM.SCOMP0 = 0; // 10) 8 sample clocks
        ADC1->ULLMEM.CPU_INT.IMASK = 0; // 11) no interrupt
}
uint32_t ADCin(void){
    // write code to sample ADC1 channel 5, PB18 once
    // return digital result (0 to 4095)
    ADC1->ULLMEM.CTL0 |= 0x00000001;
    ADC1->ULLMEM.CTL1 |= 0x00000100;
    uint32_t volatile delay = ADC1->ULLMEM.STATUS; // time to start
    while((ADC1->ULLMEM.STATUS&0x01)==0x01){};
    return ADC1->ULLMEM.MEMRES[0];
}

// your function to convert ADC sample to distance (0.001cm)
// use main2 to calibrate the system fill in 5 points in Calibration.xls
//    determine constants k1 k2 to fit Position=(k1*Data + k2)>>12
uint32_t Convert(uint32_t input){
    return ((1895*input)+13)>>12; // replace this with a linear function
    // returned values are ok
}

void OutFix(uint32_t n){
    // resolution is 0.001cm
    // n is integer 0 to 2000
    // output to ST7735 0.000cm to 2.000cm
    printf("d=%u.%u cm   ",(n/1000),(n%1000));
}
