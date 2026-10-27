#ifndef __JW01_H
#define __JW01_H
#include "stm32f10x.h"

void JW01_Init(void);			// USART3 初始化
void JW01_Process(void);		// 每 10ms 调用：预热计时 + 帧超时重同步
uint8_t JW01_IsWarmupDone(void);	// 1 = 预热完成
uint8_t JW01_GetCO2(uint16_t *ppm);	// 有新帧返回 1 并写入 *ppm
uint16_t JW01_GetCO2Value(void);	// 直接返回最近一次 ppm

#endif
