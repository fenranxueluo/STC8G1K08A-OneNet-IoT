#include "STC8G.h"
#include "Delay.h"
#include "uart.h"
#include "dht11.h"
#include "oled.h"

/* 全局变量 */
extern unsigned char led_state;
DHT11_Data dht_data;

/* 标志位 */
volatile unsigned char g_dht11_ready = 0;
volatile unsigned char g_data_updated = 0;

/* 温度湿度显示值（放大10倍存储，避免float运算） */
volatile int g_temp_x10 = 0;
volatile int g_hum_x10 = 0;

/* 系统状态 */
volatile unsigned char g_system_error = 0;
#define ERROR_WIFI_INIT     0x01
#define ERROR_MQTT_CONN     0x02
#define ERROR_SENSOR_READ   0x04

/* 看门狗喂狗周期计数 */
static unsigned char wdt_counter = 0;

/**
 * @brief Timer0 初始化
 * @note 11.0592MHz时钟，模式1（16位定时器），50ms中断一次
 *       定时器计算公式：65536 - (11059200/12/20) = 65536 - 46080 = 19456
 */
void Timer0_Init(void) {
    TMOD &= 0xF0;
    TMOD |= 0x01;
    TH0 = 0x4B;  /* (65536 - 46080) / 256 = 76 = 0x4C, 修正为0x4B确保50ms */
    TL0 = 0x00;  /* (65536 - 46080) % 256 = 0 */
    ET0 = 1;
    TR0 = 1;
}

/**
 * @brief 看门狗初始化
 * @note 约2秒超时
 */
void WDT_Init(void) {
    WDT_CONTR = 0x27;  /* EN_WDT=1, CLR_WDT=0, IDL_WDT=0, PS2=0, PS1=1, PS0=1 */
}

/**
 * @brief 喂狗
 */
void WDT_Feed(void) {
    WDT_CONTR |= 0x10;  /* CLR_WDT位置1 */
}

/**
 * @brief 显示错误信息
 */
static void ShowError(unsigned char error_code) {
    OLED_Clear();
    OLED_ShowString(0, 0, "Error:");
    if (error_code & ERROR_WIFI_INIT) {
        OLED_ShowString(0, 2, "WiFi Failed");
    }
    if (error_code & ERROR_MQTT_CONN) {
        OLED_ShowString(0, 4, "MQTT Failed");
    }
}

/**
 * @brief 主函数
 */
void main(void) {
    unsigned char wifi_retry = 0;
    
    /* GPIO初始化 */
    /* P3.2 - DHT11数据线（输入模式） */
    P3M0 &= ~(1 << 2);
    P3M1 |= (1 << 2);
    /* P3.3 - LED控制（推挽输出） */
    P3M0 |= (1 << 3);
    P3M1 &= ~(1 << 3);
    /* P5.4/P5.5 - I2C OLED（准双向模式） */
    P5M0 &= ~0x30;
    P5M1 &= ~0x30;

    UART_Init();
    OLED_Init();
    Timer0_Init();
    WDT_Init();
    EA = 1;

    /* 初始化显示 */
    OLED_ShowString(0, 0, "Init...");
    
    /* WiFi初始化，带重试 */
    for (wifi_retry = 0; wifi_retry < 3; wifi_retry++) {
        if (ESP8266_Init()) {
            break;
        }
        Delay_ms(2000);
    }
    
    if (wifi_retry >= 3) {
        g_system_error |= ERROR_WIFI_INIT;
        ShowError(g_system_error);
        /* 继续运行，尝试离线模式 */
    } else {
        /* 订阅控制主题 */
        if (!ESP8266_SubscribeSetTopic()) {
            g_system_error |= ERROR_MQTT_CONN;
        }
    }

    OLED_Clear();
    OLED_ShowString(0, 0, "Temp: ");
    OLED_ShowString(0, 2, "Hum: ");

    while (1) {
        /* LED控制 */
        P33 = led_state;

        /* DHT11数据采集任务 */
        if (g_dht11_ready) {
            g_dht11_ready = 0;

            if (DHT11_Read(&dht_data)) {
                /* 整数运算替代float，结果放大10倍 */
                g_temp_x10 = dht_data.temp_int * 10 + dht_data.temp_dec;
                g_hum_x10 = dht_data.hum_int * 10 + dht_data.hum_dec;
                g_data_updated = 1;

                /* 更新OLED显示 */
                OLED_ShowIntWithDecimal(40, 0, g_temp_x10, 2);
                OLED_ShowIntWithDecimal(40, 2, g_hum_x10, 2);

                /* 上传数据到OneNet（仅在无错误时） */
                if (g_system_error == 0) {
                    ESP8266_PublishMessage(g_temp_x10, g_hum_x10);
                }
            } else {
                g_system_error |= ERROR_SENSOR_READ;
            }
            
            /* 喂狗 */
            WDT_Feed();
            wdt_counter = 0;
        }
    }
}

/**
 * @brief Timer0中断服务程序
 * @note 每50ms进入一次，仅设置标志位，不执行耗时操作
 */
void Timer0_ISR(void) interrupt 1 {
    static unsigned char tick_count = 0;

    TH0 = 0x4B;
    TL0 = 0x00;

    tick_count++;
    if (tick_count >= 80) {  /* 80 * 50ms = 4000ms = 4秒 */
        tick_count = 0;
        g_dht11_ready = 1;
    }
    
    /* 看门狗计数 */
    wdt_counter++;
    if (wdt_counter >= 30) {  /* 1.5秒必须喂狗一次 */
        WDT_Feed();
        wdt_counter = 0;
    }
}
