#ifndef __UART_H
#define __UART_H

#include "STC8G.h"

void UART_Init(void);
unsigned char UART_SendByte(unsigned char Byte);
void UART_SendString(unsigned char *str);
unsigned char ESP8266_Init(void);
unsigned char ESP8266_SubscribeSetTopic(void);
void ESP8266_PublishMessage(int temp_x10, int hum_x10);

extern unsigned char led_state;

#endif
