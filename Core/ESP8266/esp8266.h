/*
 * esp8266.h
 *
 *  Created on: Apr 6, 2025
 *      Author: Kikkiu
 */

#ifndef ESP8266_ESP8266_H_
#define ESP8266_ESP8266_H_

#include "stm32g0xx_hal.h"

#define AT_SHORT_TIMEOUT 10
#define AT_MEDIUM_TIMEOUT 100

#define RESPONSE_MAX_SIZE 512
#define REQUEST_MAX_SIZE 64
#define WIFI_BUF_MAX_SIZE 256

#define UART_TIMEOUT 100	// ms
#define UART_BUFFER_SIZE 256
#define STM_UART huart1

typedef enum
{
	GET 		= 'G',
	POST 		= 'P'
} Request_t;

typedef enum
{
	ERR 		= 0,
	TIMEOUT 	= 1,
	OK 			= 2,
	FOUND 		= 3
} Response_t;

typedef struct
{
	char 		IP[15];
	char 		SSID[32];
	char		pw[64];
	char		buf[RESPONSE_MAX_SIZE];
	uint32_t	uptime;
} WIFI_t;

typedef struct
{
	WIFI_t* 	wifi;
	uint8_t 	connection_number;
 	Request_t	request_type;
	char		request[REQUEST_MAX_SIZE];
	uint32_t	request_size;
	char		response_buffer[RESPONSE_MAX_SIZE];
} Connection_t;

#define ESP_RST_PORT
#define ESP_RST_PIN

void ESP8266_Init(void);
void DMA_Callback(void);
void ESP8266_ClearBuffer(void);
char* ESP8266_GetBuffer(void);
void ESP8266_HardwareReset(void);
Response_t ESP8266_ATReset(void);
Response_t ESP8266_CheckAT(void);

Response_t ESP8266_WaitForString(char* str, uint32_t timeout);
Response_t ESP8266_WaitKeepString(char* str, uint32_t timeout);

HAL_StatusTypeDef ESP8266_SendATCommandNoResponse(char* cmd, size_t size, uint32_t timeout);
Response_t ESP8266_SendATCommandResponse(char* cmd, size_t size, uint32_t timeout);
Response_t ESP8266_SendATCommandKeepString(char* cmd, size_t size, uint32_t timeout);

Response_t WIFI_Connect(WIFI_t* wifi);
Response_t WIFI_GetConnectionInfo(WIFI_t* wifi);
Response_t WIFI_SetCWMODE(char* mode);
Response_t WIFI_SetCIPMUX(char* mux);
Response_t WIFI_SetCIPSERVER(char* server_port);

Response_t WIFI_ReceiveRequest(WIFI_t* wifi, Connection_t* conn, uint32_t timeout);
Response_t WIFI_SendResponse(Connection_t* conn, char* status_code, char* body, uint32_t body_length);
Response_t WIFI_GetTime(WIFI_t* wifi, char* time_buf, size_t buf_size);
Response_t WIFI_EnableNTPServer(WIFI_t* wifi, int8_t time_offset);
char* WIFI_RequestHasKey(Connection_t* conn, char* desired_key);
char* WIFI_RequestKeyHasValue(Connection_t* conn, char* request_key_ptr, char* value);
char* WIFI_GetKeyValue(Connection_t* conn, char* request_key_ptr, uint32_t* value_size);

#endif /* ESP8266_ESP8266_H_ */
