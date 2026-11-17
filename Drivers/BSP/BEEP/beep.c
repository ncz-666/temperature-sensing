#include "beep.h"

static TIM_HandleTypeDef htim_beep;

/**
 * @brief  蜂鸣器初始化：配置 GPIO 和 TIM PWM
 * @param  无
 * @retval 无
 */
void BEEP_Init(void)
{
    /* 1. 开启 GPIO 和定时器时钟 */
    __HAL_RCC_GPIOA_CLK_ENABLE();   // 若修改引脚，需修改此处时钟
    __HAL_RCC_TIM2_CLK_ENABLE();    // 若修改定时器，需修改此处时钟

    /* 2. 配置 GPIO 为复用推挽输出 */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin   = BEEP_GPIO_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BEEP_GPIO_PORT, &GPIO_InitStruct);

    /* 3. 配置定时器基础参数 */
    htim_beep.Instance               = BEEP_TIM;
    htim_beep.Init.Prescaler         = (BEEP_TIM_CLK / 1000000) - 1;  // 计数频率 1MHz
    htim_beep.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim_beep.Init.Period            = 1000 - 1;                      // 默认 1kHz
    htim_beep.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim_beep.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    HAL_TIM_PWM_Init(&htim_beep);

    /* 4. 配置 PWM 通道（占空比 50%） */
    TIM_OC_InitTypeDef sConfigOC = {0};
    sConfigOC.OCMode     = TIM_OCMODE_PWM1;
    sConfigOC.Pulse      = 500;              // 初始占空比 50%
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
    HAL_TIM_PWM_ConfigChannel(&htim_beep, &sConfigOC, BEEP_TIM_CHANNEL);

    /* 5. 默认关闭蜂鸣器 */
    BEEP_Off();
}

/**
 * @brief  开启蜂鸣器（开始输出 PWM）
 */
void BEEP_On(void)
{
    HAL_TIM_PWM_Start(&htim_beep, BEEP_TIM_CHANNEL);
}

/**
 * @brief  关闭蜂鸣器（停止 PWM，输出低电平）
 */
void BEEP_Off(void)
{
    HAL_TIM_PWM_Stop(&htim_beep, BEEP_TIM_CHANNEL);
    HAL_GPIO_WritePin(BEEP_GPIO_PORT, BEEP_GPIO_PIN, GPIO_PIN_RESET);
}

/**
 * @brief  设置蜂鸣器发声频率（音调）
 * @param  freq: 频率值，单位 Hz（例如 1000 表示 1kHz）
 */
void BEEP_SetFrequency(uint32_t freq)
{
    if (freq == 0) return;

    /* 假设计数频率为 1MHz，则周期值 = 1000000 / freq - 1 */
    uint32_t period = (1000000 / freq) - 1;
    if (period < 1) period = 1;
    if (period > 0xFFFF) period = 0xFFFF;   // 16位定时器最大值

    /* 先停止 PWM 输出 */
    HAL_TIM_PWM_Stop(&htim_beep, BEEP_TIM_CHANNEL);

    /* 修改自动重装值（周期） */
    __HAL_TIM_SET_AUTORELOAD(&htim_beep, period);

    /* 重新设置占空比为 50% */
    __HAL_TIM_SET_COMPARE(&htim_beep, BEEP_TIM_CHANNEL, period / 2);

    /* 重新启动 PWM */
    HAL_TIM_PWM_Start(&htim_beep, BEEP_TIM_CHANNEL);
}

/**
 * @brief  蜂鸣器响一声（指定频率和持续时间）
 * @param  freq: 频率（Hz）
 * @param  duration_ms: 持续时间（毫秒）
 */
void BEEP_Beep(uint32_t freq, uint32_t duration_ms)
{
    BEEP_SetFrequency(freq);
    BEEP_On();
    HAL_Delay(duration_ms);
    BEEP_Off();
}


