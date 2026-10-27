#ifndef __AD_H
#define __AD_H
#include "stm32f10x.h"

/* 土壤湿度标定值（电容式：越干电压越高、越湿电压越低），需现场实测微调 */
#define SOIL_ADC_AIR		3200	// 空气中(全干)实测 ADC 值
#define SOIL_ADC_WATER		1200	// 浸水(全湿)实测 ADC 值

void AD_Init(void);
uint16_t AD_GetValue(void);		// 返回 0~4095 原始值
uint8_t AD_GetSoilPercent(void);	// 返回 0~100 土壤湿度百分比

#endif
