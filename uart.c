#include "STC8G.h"
#include "Delay.h"
#include <string.h>

/* 全局变量 */
unsigned char led_state = 0;

/* 串口缓冲区 */
#define ESP8266_BUFFER_SIZE 128
static unsigned char xdata ESP8266_Buffer[ESP8266_BUFFER_SIZE];
static unsigned char buffer_index = 0;
static unsigned char g_cmd_response_ready = 0;

/* OneNet配置 - 存储在Flash节省RAM */
#define NAME "your_device_name"
#define ID "your_device_id"
#define TOKEN "your_token"

/* WiFi配置 - 请根据实际修改 */
#define WIFI_SSID "your_wifi_ssid"
#define WIFI_PASS "your_wifi_password"

/* MQTT服务器配置 */
#define ONENET_SERVER "mqtts.heclouds.com"
#define ONENET_PORT 1883

/**
 * @brief UART初始化 115200bps@11.0592MHz
 */
void UART_Init(void) {
    SCON = 0x50;
    AUXR |= 0x40;
    AUXR &= 0xFE;
    TMOD &= 0x0F;
    TL1 = 0xE8;
    TH1 = 0xFF;
    ET1 = 0;
    TR1 = 1;
    P_SW1 &= ~0xc0;
}

/**
 * @brief 发送单个字节（带超时保护）
 * @param Byte 要发送的字节
 * @return 0-超时失败，1-成功
 */
unsigned char UART_SendByte(unsigned char Byte) {
    unsigned int timeout = 10000;
    ES = 0;
    TI = 0;
    SBUF = Byte;
    while (TI == 0 && --timeout);
    TI = 0;
    ES = 1;
    return timeout > 0 ? 1 : 0;
}

/**
 * @brief 发送字符串
 */
void UART_SendString(unsigned char *str) {
    while (*str != '\0') {
        UART_SendByte(*str);
        Delay_us(5);
        str++;
    }
}

/**
 * @brief 等待AT指令响应
 * @param expect 期望的响应字符串
 * @param timeout_ms 超时时间（毫秒）
 * @return 0-超时，1-成功
 */
static unsigned char ESP8266_WaitResponse(unsigned char *expect, unsigned int timeout_ms) {
    unsigned int count = 0;
    g_cmd_response_ready = 0;

    while (count < timeout_ms) {
        if (g_cmd_response_ready) {
            g_cmd_response_ready = 0;
            if (strstr((char*)ESP8266_Buffer, (char*)expect) != NULL) {
                return 1;
            }
        }
        Delay_ms(1);
        count++;
    }
    return 0;
}

/**
 * @brief 发送AT指令并等待响应
 * @param cmd 指令字符串
 * @param expect 期望响应
 * @param timeout_ms 超时时间
 * @return 0-失败，1-成功
 */
static unsigned char ESP8266_SendCmd(unsigned char *cmd, unsigned char *expect, unsigned int timeout_ms) {
    UART_SendString(cmd);
    return ESP8266_WaitResponse(expect, timeout_ms);
}

/**
 * @brief 整型转字符串（轻量级，替代sprintf）
 * @param num 整数
 * @param buf 输出缓冲区
 * @return 字符串长度
 */
static unsigned char IntToString(int num, unsigned char *buf) {
    unsigned char i = 0, j;
    unsigned char is_negative = 0;
    unsigned char temp[8];
    
    if (num < 0) {
        is_negative = 1;
        num = -num;
    }
    
    do {
        temp[i++] = num % 10 + '0';
        num /= 10;
    } while (num > 0);
    
    if (is_negative) {
        temp[i++] = '-';
    }
    
    /* 反转 */
    for (j = 0; j < i; j++) {
        buf[j] = temp[i - 1 - j];
    }
    buf[i] = '\0';
    return i;
}

/**
 * @brief 构建WiFi连接命令
 */
static void BuildWiFiCmd(unsigned char *buf) {
    unsigned char i = 0;
    unsigned char *ssid = WIFI_SSID;
    unsigned char *pass = WIFI_PASS;
    
    buf[i++] = 'A'; buf[i++] = 'T'; buf[i++] = '+';
    buf[i++] = 'C'; buf[i++] = 'W'; buf[i++] = 'J';
    buf[i++] = 'A'; buf[i++] = 'P'; buf[i++] = '=';
    buf[i++] = '"';
    while (*ssid) buf[i++] = *ssid++;
    buf[i++] = '"'; buf[i++] = ','; buf[i++] = '"';
    while (*pass) buf[i++] = *pass++;
    buf[i++] = '"'; 
    buf[i++] = '\r'; buf[i++] = '\n';
    buf[i] = '\0';
}

/**
 * @brief 构建MQTT配置命令
 */
static void BuildMQTTConfigCmd(unsigned char *buf) {
    unsigned char i = 0;
    unsigned char *name = NAME;
    unsigned char *id = ID;
    unsigned char *token = TOKEN;
    
    buf[i++] = 'A'; buf[i++] = 'T'; buf[i++] = '+';
    buf[i++] = 'M'; buf[i++] = 'Q'; buf[i++] = 'T';
    buf[i++] = 'T'; buf[i++] = 'U'; buf[i++] = 'S';
    buf[i++] = 'E'; buf[i++] = 'R'; buf[i++] = 'C';
    buf[i++] = 'F'; buf[i++] = 'G'; buf[i++] = '=';
    buf[i++] = '0'; buf[i++] = ','; buf[i++] = '1';
    buf[i++] = ','; buf[i++] = '"';
    while (*name) buf[i++] = *name++;
    buf[i++] = '"'; buf[i++] = ','; buf[i++] = '"';
    while (*id) buf[i++] = *id++;
    buf[i++] = '"'; buf[i++] = ','; buf[i++] = '"';
    while (*token) buf[i++] = *token++;
    buf[i++] = '"'; buf[i++] = ','; buf[i++] = '0';
    buf[i++] = ','; buf[i++] = '0'; buf[i++] = ',';
    buf[i++] = '"'; buf[i++] = '"'; 
    buf[i++] = '\r'; buf[i++] = '\n';
    buf[i] = '\0';
}

/**
 * @brief 构建MQTT连接命令
 */
static void BuildMQTTConnCmd(unsigned char *buf) {
    unsigned char i = 0;
    unsigned char *server = ONENET_SERVER;
    
    buf[i++] = 'A'; buf[i++] = 'T'; buf[i++] = '+';
    buf[i++] = 'M'; buf[i++] = 'Q'; buf[i++] = 'T';
    buf[i++] = 'T'; buf[i++] = 'C'; buf[i++] = 'O';
    buf[i++] = 'N'; buf[i++] = 'N'; buf[i++] = '=';
    buf[i++] = '0'; buf[i++] = ','; buf[i++] = '"';
    while (*server) buf[i++] = *server++;
    buf[i++] = '"'; buf[i++] = ','; 
    buf[i++] = '1'; buf[i++] = '8'; buf[i++] = '8';
    buf[i++] = '3'; buf[i++] = ','; buf[i++] = '1';
    buf[i++] = '\r'; buf[i++] = '\n';
    buf[i] = '\0';
}

/**
 * @brief ESP8266初始化
 * @return 0-失败，1-成功
 */
unsigned char ESP8266_Init(void) {
    unsigned char xdata cmd_buf[128];
    unsigned char retry = 0;

    /* 复位模块 */
    UART_SendString("AT+RST\r\n");
    Delay_ms(2000);

    /* 测试AT指令 */
    for (retry = 0; retry < 3; retry++) {
        if (ESP8266_SendCmd("AT\r\n", "OK", 1000)) {
            break;
        }
    }
    if (retry >= 3) return 0;

    /* 设置STA模式 */
    if (!ESP8266_SendCmd("AT+CWMODE=1\r\n", "OK", 1000)) return 0;

    /* 启用DHCP */
    if (!ESP8266_SendCmd("AT+CWDHCP=1,1\r\n", "OK", 1000)) return 0;

    /* 连接WiFi */
    BuildWiFiCmd(cmd_buf);
    for (retry = 0; retry < 3; retry++) {
        UART_SendString(cmd_buf);
        if (ESP8266_WaitResponse("OK", 10000)) {
            break;
        }
        Delay_ms(1000);
    }
    if (retry >= 3) return 0;

    /* 配置MQTT */
    BuildMQTTConfigCmd(cmd_buf);
    if (!ESP8266_SendCmd(cmd_buf, "OK", 2000)) return 0;

    /* 连接MQTT服务器 */
    BuildMQTTConnCmd(cmd_buf);
    for (retry = 0; retry < 3; retry++) {
        UART_SendString(cmd_buf);
        if (ESP8266_WaitResponse("OK", 5000)) {
            break;
        }
        Delay_ms(1000);
    }
    
    return retry < 3 ? 1 : 0;
}

/**
 * @brief 订阅控制主题
 * @return 0-失败，1-成功
 */
unsigned char ESP8266_SubscribeSetTopic(void) {
    unsigned char xdata cmd_buf[128];
    unsigned char i = 0;
    unsigned char *id = ID;
    unsigned char *name = NAME;
    
    cmd_buf[i++] = 'A'; cmd_buf[i++] = 'T'; cmd_buf[i++] = '+';
    cmd_buf[i++] = 'M'; cmd_buf[i++] = 'Q'; cmd_buf[i++] = 'T';
    cmd_buf[i++] = 'T'; cmd_buf[i++] = 'S'; cmd_buf[i++] = 'U';
    cmd_buf[i++] = 'B'; cmd_buf[i++] = '='; cmd_buf[i++] = '0';
    cmd_buf[i++] = ','; cmd_buf[i++] = '"';
    cmd_buf[i++] = '$'; cmd_buf[i++] = 's'; cmd_buf[i++] = 'y';
    cmd_buf[i++] = 's'; cmd_buf[i++] = '/';
    while (*id) cmd_buf[i++] = *id++;
    cmd_buf[i++] = '/';
    while (*name) cmd_buf[i++] = *name++;
    cmd_buf[i++] = '/'; cmd_buf[i++] = 't'; cmd_buf[i++] = 'h';
    cmd_buf[i++] = 'i'; cmd_buf[i++] = 'n'; cmd_buf[i++] = 'g';
    cmd_buf[i++] = '/'; cmd_buf[i++] = 'p'; cmd_buf[i++] = 'r';
    cmd_buf[i++] = 'o'; cmd_buf[i++] = 'p'; cmd_buf[i++] = 'e';
    cmd_buf[i++] = 'r'; cmd_buf[i++] = 't'; cmd_buf[i++] = 'y';
    cmd_buf[i++] = '/'; cmd_buf[i++] = 's'; cmd_buf[i++] = 'e';
    cmd_buf[i++] = 't'; cmd_buf[i++] = '"'; cmd_buf[i++] = ',';
    cmd_buf[i++] = '0'; 
    cmd_buf[i++] = '\r'; cmd_buf[i++] = '\n';
    cmd_buf[i] = '\0';
    
    return ESP8266_SendCmd(cmd_buf, "OK", 2000);
}

/**
 * @brief 发布温湿度数据
 * @param temp_x10 温度值（放大10倍）
 * @param hum_x10 湿度值（放大10倍）
 */
void ESP8266_PublishMessage(int temp_x10, int hum_x10) {
    unsigned char xdata msg_buf[160];
    unsigned char i = 0;
    unsigned char temp_buf[8], hum_buf[8];
    unsigned char temp_len, hum_len;
    int temp_int, temp_dec, hum_int, hum_dec;
    unsigned char *id = ID;
    unsigned char *name = NAME;
    
    /* 计算温度值 */
    temp_int = temp_x10 / 10;
    temp_dec = temp_x10 % 10;
    if (temp_dec < 0) temp_dec = -temp_dec;
    
    /* 计算湿度值 */
    hum_int = hum_x10 / 10;
    hum_dec = hum_x10 % 10;
    if (hum_dec < 0) hum_dec = -hum_dec;
    
    /* 转换为字符串 */
    temp_len = IntToString(temp_int, temp_buf);
    hum_len = IntToString(hum_int, hum_buf);
    
    /* 构建MQTT发布命令 */
    msg_buf[i++] = 'A'; msg_buf[i++] = 'T'; msg_buf[i++] = '+';
    msg_buf[i++] = 'M'; msg_buf[i++] = 'Q'; msg_buf[i++] = 'T';
    msg_buf[i++] = 'T'; msg_buf[i++] = 'P'; msg_buf[i++] = 'U';
    msg_buf[i++] = 'B'; msg_buf[i++] = '='; msg_buf[i++] = '0';
    msg_buf[i++] = ','; msg_buf[i++] = '"';
    msg_buf[i++] = '$'; msg_buf[i++] = 's'; msg_buf[i++] = 'y';
    msg_buf[i++] = 's'; msg_buf[i++] = '/';
    while (*id) msg_buf[i++] = *id++;
    msg_buf[i++] = '/';
    while (*name) msg_buf[i++] = *name++;
    msg_buf[i++] = '/'; msg_buf[i++] = 't'; msg_buf[i++] = 'h';
    msg_buf[i++] = 'i'; msg_buf[i++] = 'n'; msg_buf[i++] = 'g';
    msg_buf[i++] = '/'; msg_buf[i++] = 'p'; msg_buf[i++] = 'r';
    msg_buf[i++] = 'o'; msg_buf[i++] = 'p'; msg_buf[i++] = 'e';
    msg_buf[i++] = 'r'; msg_buf[i++] = 't'; msg_buf[i++] = 'y';
    msg_buf[i++] = '/'; msg_buf[i++] = 'p'; msg_buf[i++] = 'o';
    msg_buf[i++] = 's'; msg_buf[i++] = 't'; msg_buf[i++] = '"';
    msg_buf[i++] = ','; msg_buf[i++] = '"';
    msg_buf[i++] = '{'; msg_buf[i++] = '"'; msg_buf[i++] = 'i';
    msg_buf[i++] = 'd'; msg_buf[i++] = '"'; msg_buf[i++] = ':';
    msg_buf[i++] = '"'; msg_buf[i++] = '1'; msg_buf[i++] = '2';
    msg_buf[i++] = '3'; msg_buf[i++] = '"'; msg_buf[i++] = ',';
    msg_buf[i++] = '"'; msg_buf[i++] = 'p'; msg_buf[i++] = 'a';
    msg_buf[i++] = 'r'; msg_buf[i++] = 'a'; msg_buf[i++] = 'm';
    msg_buf[i++] = 's'; msg_buf[i++] = '"'; msg_buf[i++] = ':';
    msg_buf[i++] = '{'; msg_buf[i++] = '"'; msg_buf[i++] = 't';
    msg_buf[i++] = 'e'; msg_buf[i++] = 'm'; msg_buf[i++] = 'p';
    msg_buf[i++] = '"'; msg_buf[i++] = ':'; msg_buf[i++] = '{';
    msg_buf[i++] = '"'; msg_buf[i++] = 'v'; msg_buf[i++] = 'a';
    msg_buf[i++] = 'l'; msg_buf[i++] = 'u'; msg_buf[i++] = 'e';
    msg_buf[i++] = '"'; msg_buf[i++] = ':';
    
    /* 添加温度值 */
    {
        unsigned char j;
        for (j = 0; j < temp_len; j++) msg_buf[i++] = temp_buf[j];
        msg_buf[i++] = '.';
        msg_buf[i++] = temp_dec + '0';
    }
    
    msg_buf[i++] = '}'; msg_buf[i++] = ','; msg_buf[i++] = '"';
    msg_buf[i++] = 'h'; msg_buf[i++] = 'u'; msg_buf[i++] = 'm';
    msg_buf[i++] = '"'; msg_buf[i++] = ':'; msg_buf[i++] = '{';
    msg_buf[i++] = '"'; msg_buf[i++] = 'v'; msg_buf[i++] = 'a';
    msg_buf[i++] = 'l'; msg_buf[i++] = 'u'; msg_buf[i++] = 'e';
    msg_buf[i++] = '"'; msg_buf[i++] = ':';
    
    /* 添加湿度值 */
    {
        unsigned char j;
        for (j = 0; j < hum_len; j++) msg_buf[i++] = hum_buf[j];
        msg_buf[i++] = '.';
        msg_buf[i++] = hum_dec + '0';
    }
    
    msg_buf[i++] = '}'; msg_buf[i++] = '}'; msg_buf[i++] = '}';
    msg_buf[i++] = '"'; msg_buf[i++] = ','; msg_buf[i++] = '0';
    msg_buf[i++] = ','; msg_buf[i++] = '0'; 
    msg_buf[i++] = '\r'; msg_buf[i++] = '\n';
    msg_buf[i] = '\0';
    
    UART_SendString(msg_buf);
}

/**
 * @brief 解析控制命令
 */
static void Parse_Command(unsigned char *str) {
    unsigned char *ptr = strstr((char*)str, "\"LED\":");
    if (ptr) {
        ptr += 6;
        if (*ptr == '1') {
            led_state = 1;
        } else if (*ptr == '0') {
            led_state = 0;
        }
    }
}

/**
 * @brief 串口中断服务程序
 */
void UART1_Routine(void) interrupt 4 {
    unsigned char ch;

    if (TI) {
        TI = 0;
    }

    if (RI) {
        RI = 0;
        ch = SBUF;

        /* 缓冲区溢出保护 */
        if (buffer_index >= ESP8266_BUFFER_SIZE - 1) {
            buffer_index = 0;
        }

        ESP8266_Buffer[buffer_index++] = ch;

        /* 检测到行结束符，设置响应标志 */
        if (ch == '\n') {
            ESP8266_Buffer[buffer_index] = '\0';
            g_cmd_response_ready = 1;
            Parse_Command(ESP8266_Buffer);
            buffer_index = 0;
        }
    }
}
