/*
 * esp8266.c
 *
 *  Created on: Apr 6, 2025
 *      Author: Kikkiu
 */

#include "esp8266.h"
#include "usart.h"
#include <string.h>
#include <inttypes.h>
#include <stdio.h>

char uart_buffer[UART_BUFFER_SIZE];

Response_t ESP8266_WaitForString(char* str, uint32_t timeout)
{
	for (uint32_t i = 0; i < timeout; i++)
	{
		HAL_Delay(1);
		if (strstr(uart_buffer, str) != NULL)
		{
			ESP8266_ClearBuffer();
			return FOUND;
		}
	}

	return TIMEOUT;
}

Response_t ESP8266_WaitKeepString(char* str, uint32_t timeout)
{
	for (uint32_t i = 0; i < timeout; i++)
	{
		HAL_Delay(1);
		char* ptr = strstr(uart_buffer, str);
		if (ptr != NULL)
		{
			if (ptr - uart_buffer + 4 < UART_BUFFER_SIZE)
				*(ptr + 4) = '\0';
			return FOUND;
		}
	}

	return TIMEOUT;
}

HAL_StatusTypeDef ESP8266_SendATCommandNoResponse(char* cmd, size_t size, uint32_t timeout)
{
	return HAL_UART_Transmit(&STM_UART, (uint8_t*)cmd, size, UART_TIMEOUT);
}

Response_t ESP8266_SendATCommandResponse(char* cmd, size_t size, uint32_t timeout)
{
	if (HAL_UART_Transmit(&STM_UART, (uint8_t*)cmd, size, UART_TIMEOUT) != HAL_OK)
		return ERR;
	Response_t resp = ESP8266_WaitForString("OK", timeout);
	if (resp != ERR && resp != TIMEOUT)
		return OK;
	return resp;
}

Response_t ESP8266_SendATCommandKeepString(char* cmd, size_t size, uint32_t timeout)
{
	if (HAL_UART_Transmit(&STM_UART, (uint8_t*)cmd, size, UART_TIMEOUT) != HAL_OK)
		return ERR;
	Response_t resp = ESP8266_WaitKeepString("OK", timeout);
	if (resp != ERR && resp != TIMEOUT)
		return OK;
	return resp;
}

Response_t ESP8266_CheckAT(void)
{
	return ESP8266_SendATCommandResponse("AT\r\n", 4, AT_SHORT_TIMEOUT);
}

void ESP8266_Init(void)
{
	ESP8266_ClearBuffer();
	HAL_UARTEx_ReceiveToIdle_DMA(&STM_UART, (uint8_t*)uart_buffer, UART_BUFFER_SIZE);
}

void ESP8266_ClearBuffer(void)
{
	DMA1_Channel1->CCR &= 0x7FFE;				// disable DMA
	DMA1_Channel1->CNDTR = UART_BUFFER_SIZE;	// reset CNDTR so DMA starts writing from index 0
	DMA1_Channel1->CCR |= 0x01;					// enable DMA
	memset(uart_buffer, 255, UART_BUFFER_SIZE);
}

char* ESP8266_GetBuffer(void)
{
	return uart_buffer;
}

Response_t WIFI_Connect(WIFI_t* wifi)
{
	Response_t result = ERR;
	result = ESP8266_SendATCommandKeepString("AT+CWJAP?\r\n", 11, AT_SHORT_TIMEOUT);

	if (result != OK) return result;

	// check if the ESP is already connected to WiFi
	if (strstr(uart_buffer, "No AP") != NULL)
	{
		// ESP is not connected
		// connect the ESP to WiFi

		sprintf(wifi->command_buf, "AT+CWJAP=\"%s\",\"%s\"\r\n", wifi->SSID, wifi->pw);
		return ESP8266_SendATCommandResponse("AT+CWJAP=", 16 + strlen(wifi->SSID) + strlen(wifi->pw), 15000);
	}
	else
	{
		char* ptr = strstr(uart_buffer, "+CWJAP");
		if (ptr == NULL) return ERR;	// unknown response

		// ESP is already connected do WiFi
		// get WiFi SSID

		uint32_t SSID_start_index = (ptr + 8) - uart_buffer;

		ptr = strstr(uart_buffer, "\",\"");	// ptr -1 is the end index of the SSID
		if (ptr == NULL) return ERR;

		uint32_t SSID_end_index = (ptr - 1) - uart_buffer;
		if (SSID_end_index < SSID_start_index) return ERR;

		uint32_t SSID_size = SSID_end_index - SSID_start_index + 1;
		if (SSID_size > 128) return ERR;

		memcpy(wifi->SSID, uart_buffer + SSID_start_index, SSID_size);

		// get ESP IP
		// response structure:
		// +CIFSR:STAIP,"xxx.xxx.xxx.xxx"\r\n
		// +CIFSR:STAMAC,"xx:xx:xx:xx:xx:xx"\r\n

		ESP8266_ClearBuffer();
		result = ESP8266_SendATCommandKeepString("AT+CIFSR\r\n", 10, AT_SHORT_TIMEOUT);
		if (result != OK) return result;

		//ptr = strstr(uart_buffer, "+CIFSR:STAIP");
		ptr = strstr(uart_buffer, "\"");
		if (ptr == NULL) return ERR;

		uint32_t IP_start_index = (ptr + 1) - uart_buffer;

		ptr = strstr(uart_buffer, "\"\r\n");
		if (ptr == NULL) return ERR;

		uint32_t IP_end_index = (ptr - 1) - uart_buffer;
		if (IP_end_index < IP_start_index) return ERR;

		uint32_t IP_size = IP_end_index - IP_start_index + 1;
		if (IP_size > 128) return ERR;

		memcpy(wifi->IP, uart_buffer + IP_start_index, IP_size);

		return OK;
	}

	return result;	// ERR
}

void DMA_Callback(void)
{
	HAL_UARTEx_ReceiveToIdle_DMA(&STM_UART, (uint8_t*)uart_buffer, UART_BUFFER_SIZE);
}

void ESP8266_HardwareReset(void)
{

}

Response_t ESP8266_ATReset(void)
{
	Response_t resp = ESP8266_SendATCommandResponse("AT+RST\r\n", 8, AT_SHORT_TIMEOUT);
	ESP8266_ClearBuffer();
	return resp;
}


Response_t WIFI_SetCWMODE(char* mode)
{
	if (*mode < '0' || *mode > '3')
		return ERR;

	char cwmode[13];
	strcpy(cwmode, "AT+CWMODE=");
	strcat(cwmode, mode);
	strcat(cwmode, "\r\n");
	return ESP8266_SendATCommandResponse(cwmode, 13, 100);
}

Response_t WIFI_SetCIPMUX(char* mux)
{
	if (*mux != '0' && *mux != '1')
		return ERR;

	char cipmux[13];
	strcpy(cipmux, "AT+CIPMUX=");
	strcat(cipmux, mux);
	strcat(cipmux, "\r\n");
	return ESP8266_SendATCommandResponse(cipmux, 13, 100);
}

Response_t WIFI_SetCIPSERVER(char* server_port)
{
	uint32_t string_length = strlen(server_port);
	if (*server_port < '0' || string_length > 5)
		return ERR;

	char cipserver[19];
	strcpy(cipserver, "AT+CIPSERVER=1,");
	strcat(cipserver, server_port);
	strcat(cipserver, "\r\n");
	return ESP8266_SendATCommandResponse(cipserver, 19, 100);
}

Response_t WIFI_WaitRequest(Connection_t* conn, uint32_t timeout)
{
	char* ptr = NULL;
	for (uint32_t i = 0; i < timeout; i++)
	{
		HAL_Delay(1);
		ptr = strstr(uart_buffer, "+IPD,");
		if (ptr != NULL)
		{
			conn->connection_number = *(ptr + 5) - '0';

			ptr = strstr(uart_buffer, ":");
			if (ptr == NULL) return ERR;

			conn->request_type = *(ptr + 1);

			ptr = strstr(uart_buffer, " /");
			if (ptr == NULL) return ERR;

			uint32_t request_start_index = (ptr + 1) - uart_buffer;

			ptr = strstr(uart_buffer, " HTTP");
			if (ptr == NULL) return ERR;

			uint32_t request_end_index = (ptr - 1) - uart_buffer;
			if (request_end_index < request_start_index) return ERR;

			uint32_t request_size = request_end_index - request_start_index + 1;
			if (request_size > REQUEST_MAX_SIZE) return ERR;

			memset(conn->request, 0, REQUEST_MAX_SIZE);
			memcpy(conn->request, uart_buffer + request_start_index, request_size);
			ESP8266_ClearBuffer();
			return FOUND;
		}
	}

	return TIMEOUT;
}

Response_t WIFI_SendResponse(Connection_t* conn, char* status_code, char* body, uint32_t body_length)
{
	Response_t AT_status = ERR;

	// calculate width in characters of the body length and connection number
	memset(conn->response_buffer, 0, RESPONSE_MAX_SIZE);
	sprintf(conn->response_buffer, "%" PRIu32, body_length);
	uint32_t body_length_width = strlen(conn->response_buffer);
	memset(conn->response_buffer, 0, RESPONSE_MAX_SIZE);
	sprintf(conn->response_buffer, "%d", conn->connection_number);
	uint32_t connection_number_width = strlen(conn->response_buffer);
	uint32_t status_code_width = strlen(status_code);

	uint32_t total_response_length = 79 + connection_number_width + body_length_width + status_code_width + body_length;
	memset(conn->response_buffer, 0, RESPONSE_MAX_SIZE);
	sprintf(conn->response_buffer, "%" PRIu32, total_response_length);
	uint32_t total_response_length_width = strlen(conn->response_buffer);

	uint32_t cipsend_length = 14 + connection_number_width + total_response_length_width;
	if (cipsend_length > RESPONSE_MAX_SIZE) return ERR;
	if (total_response_length > RESPONSE_MAX_SIZE) return ERR;

	sprintf(conn->response_buffer, "AT+CIPSEND=%d,%" PRIu32 "\r\n", conn->connection_number, total_response_length);
	AT_status = ESP8266_SendATCommandResponse(conn->response_buffer, cipsend_length, 100);

	memset(conn->response_buffer, 0, RESPONSE_MAX_SIZE);

	sprintf(conn->response_buffer, "HTTP/1.0 %s\nContent-Type: text/plain\nContent-Length: %" PRIu32 "\nConnection: keep-alive\n\n%s", status_code, body_length, body);
	HAL_UART_Transmit(&STM_UART, (uint8_t*)conn->response_buffer, total_response_length, 250);

	//ESP8266_SendATCommandResponse("AT+CIPSEND=0,94\r\n", 94, 100);
	//HAL_UART_Transmit(&huart1, "HTTP/1.0 200 OK\nContent-Type: text/plain\nContent-Length: 6\nConnection: keep-alive\n\nHello!", 94, 100);

	return AT_status;
}
