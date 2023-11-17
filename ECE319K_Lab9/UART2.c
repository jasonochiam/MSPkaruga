/* UART2.c
 * Your name
 * Data:
 * PA22 UART2 Rx from other microcontroller PA8 UART1 Tx<br>
 */


#include <ti/devices/msp/msp.h>
#include "UART2.h"
#include "../inc/Clock.h"
#include "../inc/LaunchPad.h"
#include "FIFO1.h"
#define PA8INDEX  18 // UART1_TX  SPI0_CS0  UART0_RTS TIMA0_C0  TIMA1_C0N
#define PA9INDEX  19 // UART1_RX  SPI0_PICO UART0_CTS TIMA0_C1  RTC_OUT   TIMA0_C0N TIMA1_C1N CLK_OUT
#define PA22INDEX 46 // UART2_RX  TIMG8_C1  UART1_RTS TIMA0_C1  CLK_OUT   TIMA0_C0N TIMG6_C1

uint32_t LostData;

// power Domain PD0
// for 80MHz bus clock, UART clock is ULPCLK 40MHz
// initialize UART2 for 2000 baud rate
// no transmit, interrupt on receive timeout
void UART2_Init(void){
    UART2->GPRCM.RSTCTL = 0xB1000003;
    UART2->GPRCM.PWREN = 0x26000001;
    Clock_Delay(24); // time for uart to power up
    IOMUX->SECCFG.PINCM[PA22INDEX]  = 0x00040082;
    Fifo1_Init();
    UART2->CLKSEL = 0x08; // bus clock
    UART2->CLKDIV = 0x00; // no divide
    UART2->CTL0 &= ~0x01; // disable UART2
    UART2->CTL0 = 0x00020018;
    // assumes an 80 MHz bus clock
    UART2->IBRD = 1250;//   divider = 21+45/64 = 21.703125
    UART2->FBRD = 0; // baud =2,500,000/21.703125 = 115,191
    UART2->LCRH = 0x00000030;
    UART2->CPU_INT.IMASK = 0x0001;
    // bit 11 TXINT
    // bit 10 RXINT
    // bit 0  Receive timeout
    UART2->IFLS = 0x0422;
    // bits 11-8 RXTOSEL receiver timeout select 4 (0xF highest)
    // bits 6-4  RXIFLSEL 2 is greater than or equal to half
    // bits 2-0  TXIFLSEL 2 is less than or equal to half
    NVIC->ICPR[0] = 1<<14; // UART2 is IRQ 14
    NVIC->ISER[0] = 1<<14;
    NVIC->IP[3] = (NVIC->IP[3]&(~0x00FF0000))|(2<<22);    // priority (bits 31,30)
    UART2->CTL0 |= 0x01; // enable UART2
}
//------------UART2_InChar------------
// Get new serial port receive data from FIFO1
// Input: none
// Output: Return 0 if the FIFO1 is empty
//         Return nonzero data from the FIFO1 if available
char UART2_InChar(void){
    return Fifo1_Get();
}


void UART2_IRQHandler(void){ uint32_t status; char letter;
  status = UART2->CPU_INT.IIDX; // reading clears bit in RIS
  if(status == 0x01){   // 0x01 receive timeout
    GPIOB->DOUTTGL31_0 = BLUE; // toggle PB22 (minimally intrusive debugging)
    GPIOB->DOUTTGL31_0 = BLUE; // toggle PB22 (minimally intrusive debugging)
    // read all inputs and put into FIFO1
    while((UART2->STAT&0x04)==0){
                letter = UART2->RXDATA;
                Fifo1_Put(letter);
            }
    GPIOB->DOUTTGL31_0 = BLUE; // toggle PB22 (minimally intrusive debugging)
  }
}
