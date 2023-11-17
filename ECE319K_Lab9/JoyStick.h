/*
 * JoyStick.h
 *
 *  Created on: Nov 17, 2023
 *      Author: aidan
 */

#ifndef JOYSTICK_H_
#define JOYSTICK_H_

void JoyStick_Init(void);

uint32_t JoyStick_InButton(void);

void JoyStick_In(uint32_t *x, uint32_t *y);

#endif /* JOYSTICK_H_ */
