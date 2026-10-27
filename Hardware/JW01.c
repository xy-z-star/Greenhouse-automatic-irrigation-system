#include "stm32f10x.h"
#include "JW01.h"

/*
JW01-CO2-V2.2 主动上报 6 字节数据帧：
B1 = 0x2C   帧头
B2         CO2 高字节
B3         CO2 低字节    CO2 = B2*256 + B3
B4 = 0x03   固定字段
B5 = 0xFF   固定字段
B6         校验和 = (B1+B2+B3+B4+B5) 低 8 位
*/

#define JW01_WARMUP_TICKS		30000	// 30000 * 10ms = 300s = 5分钟预热（JW01 气敏元件需 5~10 分钟稳定）
#define JW01_FRAME_TIMEOUT		20		// 200ms 无新字节则重同步

static volatile uint8_t  jw_rxBuf[6];
static volatile uint8_t  jw_idx = 0;
static volatile uint8_t  jw_hasFrame = 0;
static volatile uint16_t jw_co2 = 0;
static volatile uint32_t jw_lastByteTick = 0;
static volatile uint32_t jw_warmupTick = 0;

void JW01_Init(void)
{
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;	// PB10 = USART3_TX
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;		// PB11 = USART3_RX（FT 5V 容忍）
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	USART_InitTypeDef USART_InitStructure;
	USART_InitStructure.USART_BaudRate = 9600;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_Init(USART3, &USART_InitStructure);

	USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);

	/* NVIC 分组设为 Group_2（2 位抢占 + 2 位子优先级） */
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
	NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_Init(&NVIC_InitStructure);

	USART_Cmd(USART3, ENABLE);
}

void JW01_Process(void)
{
	if (jw_lastByteTick < 0xFFFFFF00) jw_lastByteTick++;
	if (jw_idx != 0 && jw_lastByteTick > JW01_FRAME_TIMEOUT) jw_idx = 0;	// 超时重同步
	if (jw_warmupTick < JW01_WARMUP_TICKS) jw_warmupTick++;
}

uint8_t JW01_IsWarmupDone(void)
{
	return (jw_warmupTick >= JW01_WARMUP_TICKS) ? 1 : 0;
}

uint8_t JW01_GetCO2(uint16_t *ppm)
{
	if (jw_hasFrame)
	{
		*ppm = jw_co2;
		jw_hasFrame = 0;
		return 1;
	}
	return 0;
}

uint16_t JW01_GetCO2Value(void)
{
	return jw_co2;
}

void USART3_IRQHandler(void)
{
	if (USART_GetITStatus(USART3, USART_IT_RXNE) == SET)
	{
		uint8_t b = USART_ReceiveData(USART3);
		if (jw_idx == 0)
		{
			if (b == 0x2C) jw_rxBuf[jw_idx++] = b;	// 只在帧头 0x2C 时开始收
		}
		else
		{
			jw_rxBuf[jw_idx++] = b;
			if (jw_idx >= 6)
			{
				uint8_t sum = jw_rxBuf[0] + jw_rxBuf[1] + jw_rxBuf[2] + jw_rxBuf[3] + jw_rxBuf[4];
				if (jw_rxBuf[3] == 0x03 && jw_rxBuf[4] == 0xFF && jw_rxBuf[5] == sum)
				{
					uint16_t co2 = (uint16_t)jw_rxBuf[1] * 256 + jw_rxBuf[2];
					if (co2 > 0)	// 预热期气敏元件输出 CO2=0，视为无效丢弃
					{
						jw_co2 = co2;
						jw_hasFrame = 1;
					}
				}
				jw_idx = 0;	// 无论成败都重新同步
			}
		}
		jw_lastByteTick = 0;
		USART_ClearITPendingBit(USART3, USART_IT_RXNE);
	}
}
