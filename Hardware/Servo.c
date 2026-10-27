#include "stm32f10x.h"
#include "Servo.h"

/* 舵机驱动风扇：TIM3_CH1 = PA6，50Hz PWM，周期 20ms */
void Servo_Init(void)
{
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;	// PA6 = TIM3_CH1
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	TIM_InternalClockConfig(TIM3);

	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
	TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInitStructure.TIM_Period = 20000 - 1;	// 50Hz
	TIM_TimeBaseInitStructure.TIM_Prescaler = 72 - 1;	// 计数频率 1MHz
	TIM_TimeBaseInit(TIM3, &TIM_TimeBaseInitStructure);

	TIM_OCInitTypeDef TIM_OCInitStructure;
	TIM_OCStructInit(&TIM_OCInitStructure);
	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
	TIM_OCInitStructure.TIM_Pulse = 500;	// 初始 0°，避免上电乱摆
	TIM_OC1Init(TIM3, &TIM_OCInitStructure);

	TIM_Cmd(TIM3, ENABLE);
}

void Servo_SetAngle(float Angle)
{
	if (Angle < 0) Angle = 0;
	if (Angle > 180) Angle = 180;
	TIM_SetCompare1(TIM3, (uint16_t)(Angle / 180.0f * 2000 + 500));
}
