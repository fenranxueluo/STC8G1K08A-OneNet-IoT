#ifndef __OLED_H
#define __OLED_H

#include "STC8G.h"

#define OLED_ADDR 0x78

void OLED_Init(void);
void OLED_Clear(void);
void OLED_ShowChar(unsigned char x, unsigned char y, unsigned char chr);
void OLED_ShowString(unsigned char x, unsigned char y, unsigned char *str);
void OLED_ShowNum(unsigned char x, unsigned char y, unsigned int num, unsigned char len);
void OLED_ShowIntWithDecimal(unsigned char x, unsigned char y, int num, unsigned char decimal_places);

#endif