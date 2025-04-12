/*
 * valve.c
 *
 *  Created on: Apr 7, 2025
 *      Author: Kikkiu
 */

#include "irrigator.h"

Response_t AT_ExecuteRemoteATCommand(WIFI_t* wifi, Connection_t* conn, char* command_ptr)
{
	Response_t AT_status;
	if (command_ptr > conn->request + REQUEST_MAX_SIZE) return ERR;
	memcpy(wifi->buf, command_ptr, conn->request_size - (command_ptr - conn->request));

	if (strstr(wifi->buf, "\\r") || strstr(wifi->buf, "\\n"))
	{
		AT_status = WIFI_SendResponse(conn, "400 Bad Request", "Do not send \\r or \\n characters", 31);
		memset(wifi->buf, 0, RESPONSE_MAX_SIZE);
	}
	else
	{
		strcat(wifi->buf, "\r\n");	// add trailing \r\n to communicate with the ESP
		AT_status = ESP8266_SendATCommandKeepString(wifi->buf, RESPONSE_MAX_SIZE, AT_SHORT_TIMEOUT);
		memset(wifi->buf, 0, RESPONSE_MAX_SIZE);

		if (AT_status == OK)
		{
		  sprintf(wifi->buf, "DATA:\n");

		  char* esp_buffer = ESP8266_GetBuffer();
		  if (strlen(wifi->buf) + strlen(esp_buffer) > RESPONSE_MAX_SIZE) return ERR;

		  strcat(wifi->buf, ESP8266_GetBuffer());	// get ESP response
		  ESP8266_ClearBuffer();
		}
		else if (AT_status == ERR)
		  sprintf(wifi->buf, "FROM ESP: ERROR");
		else if (AT_status == TIMEOUT)
		  sprintf(wifi->buf, "ESP TIMEOUT");
		else
		  sprintf(wifi->buf, "UNKNOWN ANSWER");

		AT_status = WIFI_SendResponse(conn, "200 OK", wifi->buf, strlen(wifi->buf));

		memset(wifi->buf, 0, RESPONSE_MAX_SIZE);
	}


	return AT_status;
}

Response_t WIFI_HandleValveRequest(Connection_t* conn, Valve_t* valve1, char* key_ptr)
{
	// key_ptr = strstr(conn.request, "valve=")
	if (key_ptr - conn->request > strlen(conn->request))
		return WIFI_SendResponse(conn, "400 Bad Request", "ID non valido", 13);

	uint8_t requested_valve_id = *(key_ptr) - '0';

	if (requested_valve_id < 1 || requested_valve_id > 4)
		return WIFI_SendResponse(conn, "400 Bad Request", "ID non valido", 13);

	Valve_t* valve = valve1;
	for (uint8_t i = 0; i < 4; i++)
	{
		if (valve->id == requested_valve_id)
			break;
		if (valve->next_valve == NULL)
		{
			valve = NULL;
			break;
		}
		valve = valve->next_valve;
	}

	if (valve == NULL)
		return WIFI_SendResponse(conn, "400 Bad Request", "ID non trovato", 14);

	char* cmd_ptr = NULL;
	if ((cmd_ptr = strstr(conn->request, "cmd=")) == NULL)
		return WIFI_SendResponse(conn, "400 Bad Request", "Comando non trovato", 19);

	uint32_t cmd_index = cmd_ptr + 4 - conn->request;
	if (cmd_index > strlen(conn->request))
		return WIFI_SendResponse(conn, "400 Bad Request", "Comando non valido", 18);

	uint32_t cmd_size = strlen(conn->request) - (cmd_index);
	memset(conn->response_buffer, 0, RESPONSE_MAX_SIZE);
	memcpy(conn->response_buffer, conn->request + cmd_index, cmd_size);

	if (strstr(conn->response_buffer, "open"))
	{
		VALVE_Open(valve);
		return WIFI_SendResponse(conn, "200 OK", "Valvola aperta", 14);
	}
	else if(strstr(conn->response_buffer, "close"))
	{
		VALVE_Close(valve);
		return WIFI_SendResponse(conn, "200 OK", "Valvola chiusa", 14);
	}
	else
		return WIFI_SendResponse(conn, "400 Bad Request", "Comando non riconosciuto", 24);
}

void VALVE_Init(Valve_t* valve, uint8_t id, GPIO_TypeDef* valve_port, uint16_t valve_pin)
{
	valve->id = id;
	valve->isOpen = 0;
	valve->gpio_port = valve_port;
	valve->gpio_pin = valve_pin;
}

void VALVE_Open(Valve_t* valve)
{
	valve->isOpen = 1;
	HAL_GPIO_WritePin(valve->gpio_port, valve->gpio_pin, 1);
}

void VALVE_Close(Valve_t* valve)
{
	valve->isOpen = 0;
	HAL_GPIO_WritePin(valve->gpio_port, valve->gpio_pin, 0);
}

