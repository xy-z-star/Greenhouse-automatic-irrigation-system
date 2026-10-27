#ifndef __SERVO_H
#define __SERVO_H
#include "stm32f10x.h"

void Servo_Init(void);
void Servo_SetAngle(float Angle);	// 0~180 度

#endif
