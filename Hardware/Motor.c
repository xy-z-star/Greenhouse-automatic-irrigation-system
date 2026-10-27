#include "stm32f10x.h"
#include "Motor.h"

/* 灌溉电机：PA7 高电平全速（经 L298N/继电器驱动） */
void Motor_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	GPIO_ResetBits(GPIOA, GPIO_Pin_7);	// 初始停
}
void Motor_ON(void)  { GPIO_SetBits(GPIOA, GPIO_Pin_7); }
void Motor_OFF(void) { GPIO_ResetBits(GPIOA, GPIO_Pin_7); }
