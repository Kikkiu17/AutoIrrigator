/*
 * valve.c
 *
 *  Created on: Apr 7, 2025
 *      Author: Kikkiu
 */

#include "irrigator.h"

Response_t AT_HandleRemoteATCommand(WIFI_t* wifi, Connection_t* conn, char* command_ptr)
{
	Response_t AT_status;
	memcpy(wifi->command_buf, command_ptr, strlen(conn->request) - (command_ptr - conn->request));

	if (strstr(wifi->command_buf, "\\r") || strstr(wifi->command_buf, "\\n"))
	{
		AT_status = WIFI_SendResponse(conn, "400 Bad Request", "Do not send \\r or \\n characters", 31);
		memset(wifi->command_buf, 0, 112);
	}
	else
	{
		strcat(wifi->command_buf, "\r\n");	// add trailing \r\n to communicate with the ESP
		AT_status = ESP8266_SendATCommandKeepString(wifi->command_buf, RESPONSE_MAX_SIZE, 100);
		memset(wifi->command_buf, 0, RESPONSE_MAX_SIZE);

		if (AT_status == OK)
		{
		  sprintf(wifi->command_buf, "DATA:\n");
		  strcat(wifi->command_buf, ESP8266_GetBuffer());	// get ESP response
		  ESP8266_ClearBuffer();
		}
		else if (AT_status == ERR)
		  sprintf(wifi->command_buf, "FROM ESP: ERROR");
		else if (AT_status == TIMEOUT)
		  sprintf(wifi->command_buf, "ESP TIMEOUT");
		else
		  sprintf(wifi->command_buf, "UNKNOWN ANSWER");

		AT_status = WIFI_SendResponse(conn, "200 OK", wifi->command_buf, strlen(wifi->command_buf));

		memset(wifi->command_buf, RESPONSE_MAX_SIZE, 112);
	}


	return AT_status;
}

void VALVE_Init(Valve_t* valve, uint8_t id, GPIO_TypeDef* valve_port, uint16_t valve_pin)
{
	valve->id = id;
	valve->isOpen = 0;
	valve->gpio_port = valve_port;
	valve->gpio_pin = valve_pin;
}

