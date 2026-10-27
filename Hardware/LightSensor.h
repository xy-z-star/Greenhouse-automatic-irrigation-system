#ifndef __LIGHTSENSOR_H
#define __LIGHTSENSOR_H
#include "stm32f10x.h"

void Light_Init(void);
uint8_t Light_Get(void);	// 返回 DO 电平

#endif
