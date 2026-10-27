#include "stm32f10x.h"
#include "..\System\Delay.h"
#include "OLED.h"
#include "DHT22.h"
#include "LED.h"
#include "Buzzer.h"
#include "AD.h"
#include "JW01.h"
#include "Servo.h"
#include "PWM.h"
#include "Motor.h"
#include "LightSensor.h"

/* ================= 阈值宏 ================= */
#define SOIL_THRESHOLD_LOW		30		// 土壤湿度 <30% 开泵
#define SOIL_THRESHOLD_HIGH		40		// >40% 停泵（滞回，避免临界抖动）
#define PUMP_ON_ANGLE			90		// 舵机水泵开阀角度
#define PUMP_OFF_ANGLE			0		// 关阀角度
#define LIGHT_DARK_LEVEL		1		// 光敏 DO=1 视为无光照（无光照DO=1，有光照DO=0）

/* 温湿度阈值（沿用 A_1 默认值） */
#define DEFAULT_TEMP_LOW		10
#define DEFAULT_TEMP_HIGH		26
#define DEFAULT_HUMI_LOW		20
#define DEFAULT_HUMI_HIGH		70

/* ================= 环境状态机 ================= */
typedef enum {
	STATUS_NORMAL		= 0,
	STATUS_WARNING		= 1,
	STATUS_DANGER		= 2
} EnvStatus_t;

/* ================= 全局变量 ================= */
float Temperature, Humidity;
uint8_t DHT22_Error;
EnvStatus_t CurrentStatus = STATUS_NORMAL;
uint8_t TempLow, TempHigh, HumiLow, HumiHigh;

uint8_t SoilPercent = 0;
uint8_t PumpOn = 0;
uint16_t CO2ppm = 0;
uint8_t CO2Valid = 0;
uint8_t LightState = 0;		// 1 = 无光照(暗)

uint16_t SensorTimer = 0;	// DHT22 采样 2s
uint16_t ADCTimer = 0;		// 土壤采样 500ms
uint16_t OLEDTimer = 0;		// OLED 刷新 1s
uint16_t BlinkTimer = 0;	// 状态灯闪烁
uint16_t BuzzerTimer = 0;	// 蜂鸣器
uint16_t LightTimer = 0;	// 光敏采样 100ms
int16_t breathCCR = 0;		// 呼吸灯占空比
int8_t breathDir = 1;		// 呼吸方向

/* 温湿度阈值初始化 */
void Threshold_Init(void)
{
	TempLow  = DEFAULT_TEMP_LOW;
	TempHigh = DEFAULT_TEMP_HIGH;
	HumiLow  = DEFAULT_HUMI_LOW;
	HumiHigh = DEFAULT_HUMI_HIGH;
}

/* 判断环境状态（沿用 A_1 逻辑） */
EnvStatus_t EvaluateStatus(float temp, float humi)
{
	EnvStatus_t tempLevel = STATUS_NORMAL;
	EnvStatus_t humiLevel = STATUS_NORMAL;

	if (temp < TempLow)
	{
		if (temp < TempLow - 8)		tempLevel = STATUS_DANGER;
		else						tempLevel = STATUS_WARNING;
	}
	else if (temp > TempHigh)
	{
		if (temp > TempHigh + 9)	tempLevel = STATUS_DANGER;
		else						tempLevel = STATUS_WARNING;
	}
	else tempLevel = STATUS_NORMAL;

	if (humi < HumiLow)
	{
		if (humi < HumiLow - 10)	humiLevel = STATUS_DANGER;
		else						humiLevel = STATUS_WARNING;
	}
	else if (humi > HumiHigh)
	{
		if (humi > HumiHigh + 15)	humiLevel = STATUS_DANGER;
		else						humiLevel = STATUS_WARNING;
	}
	else humiLevel = STATUS_NORMAL;

	if (tempLevel == STATUS_DANGER || humiLevel == STATUS_DANGER) return STATUS_DANGER;
	if (tempLevel == STATUS_WARNING || humiLevel == STATUS_WARNING) return STATUS_WARNING;
	return STATUS_NORMAL;
}

/* 读取 DHT22 */
void UpdateSensor(void)
{
	DHT22_Error = DHT22_ReadData(&Temperature, &Humidity);
}

/* 土壤湿度 + 灌溉泵（舵机模拟水泵，滞回控制） */
void UpdateSoilPump(void)
{
	SoilPercent = AD_GetSoilPercent();
	if (!PumpOn && SoilPercent < SOIL_THRESHOLD_LOW) { Servo_SetAngle(PUMP_ON_ANGLE);  PumpOn = 1; }
	if ( PumpOn && SoilPercent > SOIL_THRESHOLD_HIGH){ Servo_SetAngle(PUMP_OFF_ANGLE); PumpOn = 0; }
}

/* 温湿度 → LED/蜂鸣器/风扇(直流电机) 联动 */
void UpdateActuator(void)
{
	switch (CurrentStatus)
	{
		case STATUS_NORMAL:
			LED1_ON(); LED2_OFF(); LED3_OFF();
			Buzzer_OFF();
			Motor_OFF();	// 风扇关
			break;
		case STATUS_WARNING:
			LED1_OFF(); LED3_OFF();
			if (BlinkTimer >= 500) { BlinkTimer = 0; LED2_Turn(); }	// 黄灯闪
			Buzzer_OFF();
			Motor_ON();		// 风扇开
			break;
		case STATUS_DANGER:
			LED1_OFF(); LED2_OFF();
			if (BlinkTimer >= 200) { BlinkTimer = 0; LED3_Turn(); }	// 红灯快闪
			if (BuzzerTimer >= 250) { BuzzerTimer = 0; Buzzer_Turn(); }	// 蜂鸣器响
			Motor_ON();		// 风扇开
			break;
	}
}

/* 呼吸灯：无光照时呼吸，有光照时熄灭 */
void UpdateBreath(void)
{
	if (LightState)
	{
		if (breathDir == 1)
		{
			if (++breathCCR >= 100) { breathCCR = 100; breathDir = -1; }
		}
		else
		{
			if (--breathCCR <= 0) { breathCCR = 0; breathDir = 1; }
		}
		PWM_SetCompare1((uint16_t)breathCCR);
	}
	else
	{
		PWM_SetCompare1(0);
		breathCCR = 0;
		breathDir = 1;
	}
}

/* OLED 显示（四行） */
void UpdateOLED(void)
{
	int16_t tempInt;
	uint8_t tempDec;
	uint16_t humiInt;
	uint8_t humiDec;

	/* Line1 空气温湿度（DHT22 失败时保留上次数据，col3 用 ! 提示） */
	OLED_ShowString(1, 1, "T:");
	tempInt = (int16_t)Temperature;
	tempDec = (uint8_t)((Temperature - tempInt) * 10);
	if (tempDec > 9) tempDec = 0;
	if (DHT22_Error) { OLED_ShowChar(1, 3, '!'); if (tempInt < 0) tempInt = -tempInt; }
	else if (tempInt < 0) { OLED_ShowChar(1, 3, '-'); tempInt = -tempInt; }
	else { OLED_ShowChar(1, 3, ' '); }
	OLED_ShowNum(1, 4, (uint16_t)tempInt, 2);
	OLED_ShowChar(1, 6, '.');
	OLED_ShowNum(1, 7, tempDec, 1);
	OLED_ShowChar(1, 8, 'C');

	OLED_ShowString(1, 10, "H:");
	humiInt = (uint16_t)Humidity;
	humiDec = (uint8_t)((Humidity - humiInt) * 10);
	if (humiDec > 9) humiDec = 0;
	OLED_ShowNum(1, 12, humiInt, 2);
	OLED_ShowChar(1, 14, '.');
	OLED_ShowNum(1, 15, humiDec, 1);
	OLED_ShowChar(1, 16, '%');

	/* Line2 土壤湿度 + 泵状态 */
	OLED_ShowString(2, 1, "Soil:");
	OLED_ShowNum(2, 6, SoilPercent, 3);
	OLED_ShowChar(2, 9, '%');
	if (PumpOn) OLED_ShowString(2, 10, "PumpON ");
	else        OLED_ShowString(2, 10, "       ");

	/* Line3 CO2 浓度 */
	if (!JW01_IsWarmupDone())
		OLED_ShowString(3, 1, "CO2: Warming.. ");
	else if (!CO2Valid)
		OLED_ShowString(3, 1, "CO2: ---- ppm ");
	else
	{
		OLED_ShowString(3, 1, "CO2:");
		OLED_ShowNum(3, 6, CO2ppm, 4);
		OLED_ShowString(3, 11, "ppm");
	}

	/* Line4 状态 */
	switch (CurrentStatus)
	{
		case STATUS_NORMAL:  OLED_ShowString(4, 1, "Normal  Fan OFF "); break;
		case STATUS_WARNING: OLED_ShowString(4, 1, "Warning Fan ON  "); break;
		case STATUS_DANGER:  OLED_ShowString(4, 1, "DANGER! ALARM   "); break;
	}
}

int main(void)
{
	OLED_Init();
	LED_Init();
	Buzzer_Init();
	Threshold_Init();
	DHT22_Init();
	AD_Init();
	JW01_Init();
	Servo_Init();
	PWM_Init();
	Motor_Init();
	Light_Init();

	OLED_ShowString(1, 1, "Auto Irrigation ");
	OLED_ShowString(2, 1, "System Starting ");
	Delay_ms(1500);
	OLED_Clear();

	Servo_SetAngle(PUMP_OFF_ANGLE);

	while (1)
	{
		Delay_ms(10);
		SensorTimer += 10;
		ADCTimer += 10;
		OLEDTimer += 10;
		BlinkTimer += 10;
		BuzzerTimer += 10;
		LightTimer += 10;

		if (SensorTimer >= 2000)
		{
			SensorTimer = 0;
			UpdateSensor();
			if (!DHT22_Error)
				CurrentStatus = EvaluateStatus(Temperature, Humidity);
		}
		if (ADCTimer >= 500)
		{
			ADCTimer = 0;
			UpdateSoilPump();
		}
		if (LightTimer >= 100)
		{
			LightTimer = 0;
			LightState = (Light_Get() == LIGHT_DARK_LEVEL) ? 1 : 0;
		}
		if (OLEDTimer >= 1000)
		{
			OLEDTimer = 0;
			UpdateOLED();
		}

		JW01_Process();
		if (JW01_GetCO2(&CO2ppm)) CO2Valid = 1;
		UpdateActuator();
		UpdateBreath();
	}
}
