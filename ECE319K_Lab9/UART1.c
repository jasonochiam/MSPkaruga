/* UART1.c
 * Your name
 * Data:
 * PA8 UART1 Tx to other microcontroller PA22 UART2 Rx
 */


#include <ti/devices/msp/msp.h>
#include "UART1.h"
#include "../inc/Clock.h"
#define PA8INDEX  18 // UART1_TX  SPI0_CS0  UART0_RTS TIMA0_C0  TIMA1_C0N


// power Domain PD0
// for 80MHz bus clock, UART clock is ULPCLK 40MHz
// initialize UART1 for 2000 baud rate
// blind, no synchronization on transmit
void UART1_Init(void){

}


//------------UART1_OutChar------------
// Output 8-bit to serial port
// blind synchronization
// 10 bit frame, 2000 baud, 5ms per frame
// Input: letter is an 8-bit ASCII character to be transferred
// Output: none
void UART1_OutChar(char data){
// simply output data to transmitter without waiting or checking status

}
