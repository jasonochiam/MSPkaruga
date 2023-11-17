/* JoyStick.c
 * MKII BoosterPack
 * Jonathan Valvano
 * November 21, 2022
 * Derived from gpio_toggle_output_LP_MSPM0G3507_nortos_ticlang
 *              adc12_single_conversion_vref_internal_LP_MSPM0G3507_nortos_ticlang
 *              adc12_single_conversion_LP_MSPM0G3507_nortos_ticlang
 */



#include <ti/devices/msp/msp.h>
#include "../inc/JoyStick.h"
#include "../inc/Clock.h"
#include "../inc/ADC.h"
#include "../inc/LaunchPad.h"
// Analog MKII  Joystick
// J1.5 joystick Select button (digital) PB24
// J1.2 joystick horizontal (X) (analog) PB19_ADC1.6
// J3.26 joystick vertical (Y) (analog)  PA17_ADC1.2

// Initialize MKII JoyStick and JoyStick button
void JoyStick_Init(void){
  //ADC_Init(ADC1,6,ADCVREF_VDDA); // x position joystick, PB19, ADC 1.6
  //ADC_Init(ADC1,2,ADCVREF_VDDA); // y position joystick, PA17, ADC 1.2
  ADC_InitDual(ADC1,2,6,ADCVREF_VDDA);

  // assume these are called from LaunchPad_Init
  //  GPIOA->GPRCM.RSTCTL = (uint32_t)0xB1000003;  // Reset GPIOA
  //  GPIOA->GPRCM.PWREN = (uint32_t)0x26000001;   // Enable power to GPIOA

  Clock_Delay(24); // time for gpio to power up

  // PINCM
  //   bit 25 is HiZ
  //   bit 20 is drive strength
  //   bit 18 is input enable control
  //   bit 17 is pull up control
  //   bit 16 is pull down control
  //   bit 7 is PC peripheral connected, enable transparent data flow
  //   bit 0 selects GPIO function
  IOMUX->SECCFG.PINCM[PA17INDEX]  = (uint32_t) 0x00040081; // y axis
  IOMUX->SECCFG.PINCM[PB19INDEX]  = (uint32_t) 0x00040081; // x axis
  IOMUX->SECCFG.PINCM[PB24INDEX]  = (uint32_t) 0x00040081; //click stick
}
#define JOYBUTTON (1<<24)
// Read JoyStick button
// Input: none
// Output: 0 if pressed, nonzero if not pressed
// TODO: does not work.
uint32_t JoyStick_InButton(void){
  return GPIOB->DIN31_0 & JOYBUTTON;
}

// Read JoyStick position
// Inputs: *x pointer to empty 32 bit unsigned variable
//         *y pointer to empty 32 bit unsigned variable
// Output: none
void JoyStick_In(uint32_t *x, uint32_t *y){
  // pass in the POINTER to where we want our sample
  ADC_InDual(ADC1,x,y);
  //*x = ADC_In(ADC0);
  //*y = ADC_In(ADC1);
}


uint32_t XData,YData,ZData, Button,Button1;
// A lower ADC value means further right for the x axis, and further down for the y axis
int mainjoystick(void){
  Clock_Init40MHz();
  LaunchPad_Init();
  ADC_InitDual(ADC1,2,6,ADCVREF_VDDA); //accelerometer X,Z (analog)
  while(1){    /* toggle on sample */
    Clock_Delay(10000000);
    ADC_InDual(ADC1,&XData,&YData);

    GPIOA->DOUT31_0 ^= RED1;
  }
}
