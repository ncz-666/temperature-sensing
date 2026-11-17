#ifndef __BEEP_H
#define __BEEP_H

#include "../Drivers/SYSTEM/sys/sys.h"

/* 蜂鸣器引脚和定时器定义，可根据需要修改 */
#define BEEP_GPIO_PORT      GPIOA
#define BEEP_GPIO_PIN       GPIO_PIN_0
#define BEEP_TIM            TIM2
#define BEEP_TIM_CHANNEL    TIM_CHANNEL_1

/* 定时器输入时钟频率（Hz），需根据实际系统时钟调整 */
/* STM32F103 使用 HAL 库默认配置时，APB1 Timer Clock = 72MHz */
#define BEEP_TIM_CLK        72000000

void BEEP_Init(void);
void BEEP_On(void);
void BEEP_Off(void);
void BEEP_SetFrequency(uint32_t freq);
void BEEP_Beep(uint32_t freq, uint32_t duration_ms);

#endif

