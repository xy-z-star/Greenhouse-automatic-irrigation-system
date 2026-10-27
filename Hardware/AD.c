#include "stm32f10x.h"
#include "AD.h"

#define SOIL_SAMPLE_N		8		// 每次取 N 次平均，滤除波动

void AD_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

	RCC_ADCCLKConfig(RCC_PCLK2_Div6);	// ADCCLK = 72/6 = 12MHz

	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;	// 模拟输入
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;		// PA4 = ADC1_IN4
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	ADC_RegularChannelConfig(ADC1, ADC_Channel_4, 1, ADC_SampleTime_55Cycles5);

	ADC_InitTypeDef ADC_InitStructure;
	ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
	ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
	ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;	// 软件触发
	ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
	ADC_InitStructure.ADC_NbrOfChannel = 1;
	ADC_InitStructure.ADC_ScanConvMode = DISABLE;
	ADC_Init(ADC1, &ADC_InitStructure);

	ADC_Cmd(ADC1, ENABLE);

	/* 校准 */
	ADC_ResetCalibration(ADC1);
	while (ADC_GetResetCalibrationStatus(ADC1) == SET);
	ADC_StartCalibration(ADC1);
	while (ADC_GetCalibrationStatus(ADC1) == SET);
}

uint16_t AD_GetValue(void)
{
	uint32_t sum = 0;
	uint8_t i;
	for (i = 0; i < SOIL_SAMPLE_N; i++)
	{
		ADC_SoftwareStartConvCmd(ADC1, ENABLE);
		while (ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET);
		sum += ADC_GetConversionValue(ADC1);
	}
	return (uint16_t)(sum / SOIL_SAMPLE_N);
}

uint8_t AD_GetSoilPercent(void)
{
	int32_t adc = AD_GetValue();
	int32_t pct = (SOIL_ADC_AIR - adc) * 100 / (SOIL_ADC_AIR - SOIL_ADC_WATER);
	if (pct < 0) pct = 0;
	if (pct > 100) pct = 100;
	return (uint8_t)pct;
}
