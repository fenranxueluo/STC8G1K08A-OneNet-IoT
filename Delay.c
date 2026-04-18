#include "STC8G.h"

/**
 * @brief 毫秒延时函数
 * @note 适用于11.0592MHz系统时钟，软件循环实现
 */
void Delay_ms(unsigned int xms) {
    unsigned char data i, j;
    while (xms) {
        i = 15;
        j = 90;
        do {
            while (--j);
        } while (--i);
        xms--;
    }
}

/**
 * @brief 微秒延时函数
 * @note 用于DHT11等短时序操作
 */
void Delay_us(unsigned int xus) {
    unsigned char data i;
    while (xus) {
        _nop_();
        _nop_();
        i = 1;
        while (--i);
        xus--;
    }
}


