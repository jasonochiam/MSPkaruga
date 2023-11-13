/*
 * ADC1.h
 *
 *  Created on: Oct 30, 2023
 *      Author: your name
 */

#ifndef ADC1_H_
#define ADC1_H_


// Initialize ADC1 channel 5, PB18
// Your measurement will be connected to PB18
// 12-bit mode, 0 to 3.3V, right justified
// software trigger, no averaging
void ADCinit(void);


// sample ADC1 channel 5, PB18 once
// return digital result (0 to 4095)
uint32_t ADCin(void);

// your function to convert ADC sample to distance (0.001cm)
// use main2 to calibrate the system fill in 5 points in Calibration.xls
//    determine constants k1 k2 to fit Position=(k1*Data + k2)>>12
uint32_t Convert(uint32_t input);

void OutFix(uint32_t n);

#endif /* ADC1_H_ */
