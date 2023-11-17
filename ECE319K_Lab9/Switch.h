/*
 * Switch.h
 *
 *  Created on: Nov 5, 2023
 *      Author: jonat
 */

#ifndef SWITCH_H_
#define SWITCH_H_

// initialize your switches
void Switch_Init(void);

// return current state of all switches as 5 bit number
//uint32_t Switch_In(void);

// return the state of the shoot switch (left)
uint32_t Shoot_In(void);

// return state of select switch (right)
uint32_t Select_In(void);

// return state of swap switch (bottom)
uint32_t Swap_In(void);

// return state of up switch (up)
uint32_t Up_In(void);

#endif /* SWITCH_H_ */
