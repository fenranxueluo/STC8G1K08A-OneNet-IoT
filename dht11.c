#include "dht11.h"
#include "Delay.h"

/* 超时计数器定义 */
#define DHT11_TIMEOUT 200

/* 读取重试次数 */
#define DHT11_READ_RETRY 3

/**
 * @brief 等待DHT11引脚状态变化，带超时保护
 * @param state 等待的状态（0或1）
 * @return 0-超时，1-成功
 */
static unsigned char DHT11_WaitPin(unsigned char state) {
    unsigned int timeout = DHT11_TIMEOUT;
    while (DHT11_PIN == state) {
        if (--timeout == 0) {
            return 0;
        }
    }
    return 1;
}

/**
 * @brief 读取DHT11传感器数据（单次）
 * @param data 存储读取结果的结构体指针
 * @return 0-失败，1-成功
 */
static unsigned char DHT11_ReadOnce(DHT11_Data *p) {
    unsigned char i, j;
    unsigned char dat[5] = {0};
    unsigned char checksum;

    /* 发送起始信号：拉低18ms */
    P3M0 &= ~(1 << 2);
    P3M1 |= (1 << 2);
    DHT11_PIN = 0;
    Delay_ms(18);
    DHT11_PIN = 1;
    Delay_us(40);

    /* 切换为输入模式 */
    P3M0 &= ~(1 << 2);
    P3M1 |= (1 << 2);

    /* 等待DHT11响应，带超时保护 */
    if (DHT11_PIN != 0) {
        return 0;
    }
    if (!DHT11_WaitPin(0)) return 0;
    if (!DHT11_WaitPin(1)) return 0;

    /* 读取40位数据 */
    for (i = 0; i < 5; i++) {
        for (j = 0; j < 8; j++) {
            /* 等待低电平结束 */
            if (!DHT11_WaitPin(0)) return 0;

            /* 延时30us判断数据位 - 关中断保护时序 */
            EA = 0;
            Delay_us(30);
            if (DHT11_PIN == 1) {
                dat[i] |= (1 << (7 - j));
                EA = 1;
                if (!DHT11_WaitPin(1)) return 0;
            } else {
                EA = 1;
            }
        }
    }

    /* 校验和检查 */
    checksum = dat[0] + dat[1] + dat[2] + dat[3];
    if (checksum != dat[4]) {
        return 0;
    }

    /* 保存数据 */
    p->hum_int = dat[0];
    p->hum_dec = dat[1];
    p->temp_int = dat[2];
    p->temp_dec = dat[3];
    p->checksum = dat[4];

    return 1;
}

/**
 * @brief 读取DHT11传感器数据（带重试）
 * @param data 存储读取结果的结构体指针
 * @return 0-失败，1-成功
 */
unsigned char DHT11_Read(DHT11_Data *p) {
    unsigned char retry;
    
    for (retry = 0; retry < DHT11_READ_RETRY; retry++) {
        if (DHT11_ReadOnce(p)) {
            return 1;
        }
        Delay_ms(100);  /* 等待后重试 */
    }
    return 0;
}