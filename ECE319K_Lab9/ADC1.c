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

}
uint32_t ADCin(void){
  // write code to sample ADC1 channel 5, PB18 once
  // return digital result (0 to 4095)
    return 42;
}

// your function to convert ADC sample to distance (0.001cm)
// use main2 to calibrate the system fill in 5 points in Calibration.xls
//    determine constants k1 k2 to fit Position=(k1*Data + k2)>>12
uint32_t Convert(uint32_t input){
  return 42; // replace this with a linear function
}

void OutFix(uint32_t n){
// resolution is 0.001cm
// n is integer 0 to 2000
// output to ST7735 0.000cm to 2.000cm

}
