/**
 ******************************************************************************
 * @file    main.c
 * @brief   温湿度监控器主程序
 *
 * 功能：DHT11实时采集温湿度 → OLED显示 → 按键设置报警阈值
 *       → 超限蜂鸣器报警(非阻塞) → 串口每1秒上报一次数据
 *
 * 硬件资源占用：
 *   AM2302/DHT22 -> PB11  (数据脚，内部上拉已启用；DHT22 需读 1 秒以上间隔)
 *   BEEP   -> PA0 TIM2_CH1 PWM (无源蜂鸣器，2.7kHz)
 *   OLED   -> PB6(SCL) PB7(SDA) 软件I2C
 *   KEY0   -> PA4 (设置/切换项)
 *   KEY1   -> PA7 (+，长按连发)
 *   KEY2   -> PB10 (-，长按连发)
 *   USART1 -> PA9(TX) PA10(RX) 115200
 *
 * 按键操作说明：
 *   正常模式：按 KEY0 进入设置模式
 *   设置模式：KEY0 切换设置项(温度上限->温度下限->湿度上限->湿度下限->退出)
 *             KEY1 数值+1(长按连发)  KEY2 数值-1(长按连发)
 *             10秒无操作自动退出设置模式
 *
 * 串口数据格式(每1秒一条，115200-8-N-1)：
 *   DATA,温度,湿度,温度上限,温度下限,湿度上限,湿度下限,报警标志\r\n
 *   例：  DATA,25.0,60.0,30.0,20.0,80.0,40.0,0
 ******************************************************************************
 */

#include <stdio.h>
#include "./SYSTEM/sys/sys.h"
#include "./SYSTEM/delay/delay.h"
#include "./SYSTEM/usart/usart.h"
#include "./BSP/KEY/key.h"
#include "./BSP/OLED/OLED.h"
#include "./BSP/DHT/DHT.h"
#include "./BSP/BEEP/beep.h"

/*===========================================================================*/
/*------------------------------ 参数配置区 --------------------------------*/

/* DHT22/AM2302 接线：改这两行即可换引脚(注意先确认新引脚时钟已使能) */
#define DHT_PORT        GPIOB
#define DHT_PIN         GPIO_PIN_11

/* 报警蜂鸣器频率(Hz)：无源蜂鸣器谐振点一般在 2.7kHz 附近，音调不对改这里 */
#define ALARM_FREQ      2700

/* 各周期(毫秒) */
#define PERIOD_DHT      2000    /* 传感器两次读取最小间隔(DHT11需2s/DHT22需1s) */
#define PERIOD_UART     1000    /* 串口上报周期 */
#define PERIOD_DISP     250     /* 屏幕刷新周期 */
#define SET_TIMEOUT_MS  10000   /* 设置模式超时自动退出 */

/* 报警节奏(毫秒)：无源蜂鸣器用开/停方波表达急促感 */
#define BEEP_ON_MS      150     /* 报警时:响150ms */
#define BEEP_OFF_MS     150     /* 报警时:停150ms */

/*===========================================================================*/
/*------------------------------ 应用状态 ----------------------------------*/

/* 设置项编号 */
enum {
    SET_T_MAX = 0,     /* 温度上限 */
    SET_T_MIN,         /* 温度下限 */
    SET_H_MAX,         /* 湿度上限 */
    SET_H_MIN,         /* 湿度下限 */
    SET_ITEM_NUM
};

/* 阈值(报警上下限)，可用按键修改 */
static float g_t_max = 30.0f;
static float g_t_min = 20.0f;
static float g_h_max = 80.0f;
static float g_h_min = 40.0f;

/* 当前测量值 */
static float g_temp = 0.0f;
static float g_hum  = 0.0f;
static uint8_t g_sensor_ok = 0;   /* 传感器数据是否有效 */

/* 模式状态 */
static uint8_t g_in_setting = 0;           /* 1=设置模式 */
static uint8_t g_set_item   = SET_T_MAX;   /* 当前正在设置的项 */
static uint32_t g_set_last_op = 0;         /* 设置模式下最后一次操作时间(超时退出用) */

/* 温湿度传感器实例（实物是 AM2302/DHT22！type 必须设 DHT22，否则读数错乱） */
static DHT_sensor g_dht = {
    .DHT_Port = DHT_PORT,
    .DHT_Pin  = DHT_PIN,
    .type     = DHT22,
    .pullUp   = GPIO_PULLUP,      /* 用内部上拉；若数据线较长建议外接4.7K上拉 */
};

/*===========================================================================*/
/*--------------------------- 内部函数声明 ----------------------------------*/

static void app_init(void);
static void app_key_handler(uint32_t now);
static int8_t key_adjust(void);            /* 设置模式下 +/- 键(支持长按连发) */
static void apply_adjust(int8_t dir);
static void app_dht_handler(uint32_t now);
static void app_display_handler(uint32_t now);
static void app_uart_handler(uint32_t now);
static void app_beep_handler(uint32_t now);
static uint8_t is_alarm(void);

/*===========================================================================*/
/*------------------------------ 主函数 ------------------------------------*/

int main(void)
{
    HAL_Init();                             /* 必须最先调用 */
    sys_stm32_clock_init(RCC_PLL_MUL9);     /* 72MHz */
    delay_init(72);                         /* delay_us/delay_ms 初始化 */
    usart_init(115200);                     /* USART1, printf重定向 */

    app_init();                             /* OLED/按键/蜂鸣器/传感器 */

    printf("\r\n=== DHT22/AM2302 Temp&Hum Monitor ===\r\n");
    printf("UART:115200-8-N-1, report every 1s\r\n\r\n");

    while (1)
    {
        uint32_t now = HAL_GetTick();

        app_key_handler(now);       /* 1. 按键处理 */
        app_dht_handler(now);       /* 2. 温湿度采集 */
        app_display_handler(now);   /* 3. OLED显示 */
        app_uart_handler(now);      /* 4. 串口上报 */
        app_beep_handler(now);      /* 5. 超限报警 */
    }
}

/*===========================================================================*/
/*------------------------------ 初始化 -------------------------------------*/

static void app_init(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();   /* DHT11所在引脚时钟(换引脚时同步修改) */

    OLED_Init();                    /* OLED自包含初始化(PB6/PB7) */
    key_init();                     /* KEY0/KEY1/KEY2 */

    BEEP_Init();
    BEEP_SetFrequency(ALARM_FREQ);  /* 设定报警音调(此函数会开启PWM，属驱动特性) */
    BEEP_Off();                     /* 立即关闭，待报警/提示时再开 */

    /* 开机显示一帧，同时验证各模块正常 */
    OLED_ShowString(0, 0, "Temp&Hum Monitor", OLED_8X16);
    OLED_ShowString(0, 16, "AM2302+OLED+BEEP", OLED_8X16);
    OLED_Update();
    BEEP_Beep(ALARM_FREQ, 100);     /* 开机"滴"一声，自检蜂鸣器 */
}

/*===========================================================================*/
/*------------------------------ 按键处理 -----------------------------------*/

/**
 * @brief 按键状态机
 *        正常模式: KEY0进设置
 *        设置模式: KEY0切项, KEY1/KEY2加减(连发)
 */
static void app_key_handler(uint32_t now)
{
    uint8_t key = key_scan(0);      /* 不支持连按，连发由key_adjust单独实现 */
    int8_t  adj;

    if (g_in_setting)
    {
        if (key == KEY0_PRES)      /* KEY0: 切换设置项，切完一轮退出 */
        {
            g_set_last_op = now;
            g_set_item++;
            if (g_set_item >= SET_ITEM_NUM)
            {
                g_in_setting = 0;  /* 退出设置模式 */
                BEEP_Beep(ALARM_FREQ, 30);   /* 短滴一声提示退出 */
            }
        }

        adj = key_adjust();        /* KEY1/KEY2: 数值调整(含长按连发) */
        if (adj != 0)
        {
            g_set_last_op = now;
            apply_adjust(adj);
        }

        /* 设置模式超时自动退出 */
        if (now - g_set_last_op >= SET_TIMEOUT_MS)
        {
            g_in_setting = 0;
        }
    }
    else
    {
        if (key == KEY0_PRES)      /* KEY0: 进入设置模式 */
        {
            g_in_setting = 1;
            g_set_item = SET_T_MAX;
            g_set_last_op = now;
            BEEP_Beep(ALARM_FREQ, 30);
        }
    }
}

/**
 * @brief  设置模式下的 +/- 键读取，支持长按连发
 * @retval +1: KEY1有效动作(加)  -1: KEY2有效动作(减)  0: 无
 * @note   按下立即返回一次；持续按住超过600ms后，每150ms连发一次
 */
static int8_t key_adjust(void)
{
    static int8_t  active = 0;      /* 当前按住的键: +1/-1/0 */
    static uint32_t hold_start = 0;
    static uint32_t last_step = 0;
    uint32_t now = HAL_GetTick();

    if (KEY1 == 0 && KEY2 == 1)
    {
        if (active == 0)            /* 刚按下：立即生效一次 */
        {
            active = 1;
            hold_start = now;
            last_step = now;
            return 1;
        }
    }
    else if (KEY2 == 0 && KEY1 == 1)
    {
        if (active == 0)
        {
            active = -1;
            hold_start = now;
            last_step = now;
            return -1;
        }
    }
    else                            /* 松开或异常：复位 */
    {
        active = 0;
    }

    /* 长按连发 */
    if (active != 0 &&
        now - hold_start >= 600 &&
        now - last_step >= 150)
    {
        last_step = now;
        return active;
    }

    return 0;
}

/**
 * @brief  将+1/-1应用到当前设置项，并做范围限制
 */
static void apply_adjust(int8_t dir)
{
    switch (g_set_item)
    {
    case SET_T_MAX:
        g_t_max += dir;
        if (g_t_max >  50.0f) g_t_max =  50.0f;
        if (g_t_max < -10.0f) g_t_max = -10.0f;
        if (g_t_max <= g_t_min) g_t_max = g_t_min + 1.0f;  /* 上限必须大于下限 */
        break;

    case SET_T_MIN:
        g_t_min += dir;
        if (g_t_min >  50.0f) g_t_min =  50.0f;
        if (g_t_min < -10.0f) g_t_min = -10.0f;
        if (g_t_min >= g_t_max) g_t_min = g_t_max - 1.0f;
        break;

    case SET_H_MAX:
        g_h_max += dir;
        if (g_h_max > 100.0f) g_h_max = 100.0f;
        if (g_h_max <   0.0f) g_h_max =   0.0f;
        if (g_h_max <= g_h_min) g_h_max = g_h_min + 1.0f;
        break;

    case SET_H_MIN:
        g_h_min += dir;
        if (g_h_min > 100.0f) g_h_min = 100.0f;
        if (g_h_min <   0.0f) g_h_min =   0.0f;
        if (g_h_min >= g_h_max) g_h_min = g_h_max - 1.0f;
        break;

    default:
        break;
    }
}

/*===========================================================================*/
/*------------------------------ DHT11采集 ----------------------------------*/

static void app_dht_handler(uint32_t now)
{
    static uint32_t last = 0;
    DHT_data d;

    if (now - last < PERIOD_DHT)     /* DHT11两次读取间隔至少2秒 */
    {
        return;
    }
    last = now;

    d = DHT_getData(&g_dht);         /* 驱动内部也有2秒限流，双保险 */

    if (d.temp < -100.0f)            /* 驱动读取失败返回-128 */
    {
        g_sensor_ok = 0;
    }
    else
    {
        g_sensor_ok = 1;
        g_temp = d.temp;
        g_hum  = d.hum;
    }
}

/*===========================================================================*/
/*------------------------------ OLED显示 -----------------------------------*/

static void app_display_handler(uint32_t now)
{
    static uint32_t last = 0;

    if (now - last < PERIOD_DISP)
    {
        return;
    }
    last = now;

    OLED_Clear();

    if (g_in_setting)
    {
        /* 设置界面：4行，">"指示当前项 */
        OLED_Printf(0,  0, OLED_8X16, "%cTMax:%dC",
                    g_set_item == SET_T_MAX ? '>' : ' ', (int)g_t_max);
        OLED_Printf(0, 16, OLED_8X16, "%cTMin:%dC",
                    g_set_item == SET_T_MIN ? '>' : ' ', (int)g_t_min);
        OLED_Printf(0, 32, OLED_8X16, "%cHMax:%d%%",
                    g_set_item == SET_H_MAX ? '>' : ' ', (int)g_h_max);
        OLED_Printf(0, 48, OLED_8X16, "%cHMin:%d%%",
                    g_set_item == SET_H_MIN ? '>' : ' ', (int)g_h_min);
    }
    else
    {
        /* 正常界面 */
        if (g_sensor_ok)
        {
            OLED_Printf(0, 0, OLED_8X16, "T:%dC  H:%d%%", (int)g_temp, (int)g_hum);
        }
        else
        {
            OLED_ShowString(0, 0, "T:--C  H:--%", OLED_8X16);
        }

        OLED_Printf(0, 16, OLED_8X16, "TLim:%d~%dC", (int)g_t_min, (int)g_t_max);
        OLED_Printf(0, 32, OLED_8X16, "HLim:%d~%d%%", (int)g_h_min, (int)g_h_max);

        /* 第4行：报警状态闪烁显示，正常时显示操作提示 */
        if (!g_sensor_ok)
        {
            OLED_ShowString(0, 48, "Sensor Error!", OLED_8X16);
        }
        else if (is_alarm())
        {
            /* 每250ms刷新一次本函数，天然形成闪烁效果 */
            static uint8_t blink = 0;
            if (blink)
            {
                OLED_ShowString(0, 48, "* ALARM *", OLED_8X16);
            }
            blink = !blink;
        }
        else
        {
            OLED_ShowString(0, 48, "KEY0:Set", OLED_8X16);
        }
    }

    OLED_Update();
}

/*===========================================================================*/
/*------------------------------ 串口上报 -----------------------------------*/

static void app_uart_handler(uint32_t now)
{
    static uint32_t last = 0;

    if (now - last < PERIOD_UART)
    {
        return;
    }
    last = now;

    /* CSV格式，方便电脑端(串口助手/serial_monitor.py)解析 */
    printf("DATA,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%d\r\n",
           g_sensor_ok ? g_temp : -128.0f,
           g_sensor_ok ? g_hum  : -128.0f,
           g_t_max, g_t_min, g_h_max, g_h_min,
           (g_sensor_ok && is_alarm()) ? 1 : 0);
}

/*===========================================================================*/
/*------------------------------ 报警蜂鸣 -----------------------------------*/

static uint8_t is_alarm(void)
{
    if (!g_sensor_ok)
    {
        return 0;
    }

    return (g_temp > g_t_max) || (g_temp < g_t_min) ||
           (g_hum  > g_h_max) || (g_hum  < g_h_min);
}

/**
 * @brief 非阻塞报警：响150ms停150ms循环。
 *        设置模式下静音，方便调阈值。
 * @note  BEEP_On/Off 只做开停，不改变音调；
 *        频率在 app_init 里已设好(ALARM_FREQ)，PWM停止后寄存器仍保留。
 */
static void app_beep_handler(uint32_t now)
{
    static uint32_t last = 0;
    static uint8_t  on = 0;

    if (g_in_setting)               /* 设置时静音 */
    {
        BEEP_Off();
        on = 0;
        return;
    }

    if (!is_alarm())
    {
        BEEP_Off();
        on = 0;
        return;
    }

    /* 交替开关，形成急促的"滴、滴、滴" */
    if (now - last >= (on ? BEEP_ON_MS : BEEP_OFF_MS))
    {
        last = now;
        on = !on;

        if (on)
        {
            BEEP_On();
        }
        else
        {
            BEEP_Off();
        }
    }
}
