#ifndef __DHT11_H
#define __DHT11_H

#include "STC8G.h"

#define DHT11_PIN P32

typedef struct {
    unsigned char hum_int;
    unsigned char hum_dec;
    unsigned char temp_int;
    unsigned char temp_dec;
    unsigned char checksum;
} DHT11_Data;

unsigned char DHT11_Read(DHT11_Data *p);

#endif
