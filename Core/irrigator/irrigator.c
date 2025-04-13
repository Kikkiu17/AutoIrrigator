/*
 * valve.c
 *
 *  Created on: Apr 7, 2025
 *      Author: Kikkiu
 */

#include "irrigator.h"

#include <stdio.h>
#include <inttypes.h>

const char valve_help_message[] =
{
		"Aiuto per \"valve\":"
		"\ncontrolla le valvole per l\'irrigazione."
		"\nStruttura richiesta: ?valve=n&cmd=x"
		"\ndove: n = ID valvola; x = comando"
		"\ncomandi disponibili:"
		"\nopen; close"
};

const char at_help_message[] =
{
		"Aiuto per \"at\":"
		"\ninvia un comando AT da far eseguire direttamente dall\'ESP."
		"\nStruttura richiesta: ?at=<comando>"
		"\n<comando> puo\' essere \"AT\" oppure \"AT+...\""
		"\nnon inviare la sequenza \\r\\n alla fine del comando"
};

Response_t AT_ExecuteRemoteATCommand(Connection_t* conn, char* command_ptr)
{
	Response_t AT_status;

	uint32_t command_size = 0;
	char* command = WIFI_GetKeyValue(conn, command_ptr, &command_size);

	if (command == NULL)
		AT_status = WIFI_SendResponse(conn, "400 Bad Request", "Comando non trovato", 31);

	if (strstr(command, "help"))
		return WIFI_SendResponse(conn, "200 OK", (char*)at_help_message, sizeof(at_help_message));

	memset(conn->wifi->buf, 0, RESPONSE_MAX_SIZE);
	memcpy(conn->wifi->buf, command, command_size);

	if (strstr(conn->wifi->buf, "\\r") || strstr(conn->wifi->buf, "\\n"))
	{
		AT_status = WIFI_SendResponse(conn, "400 Bad Request", "Non inviare i caratteri \\r e \\n", 31);
		memset(conn->wifi->buf, 0, RESPONSE_MAX_SIZE);
	}
	else
	{
		strcat(conn->wifi->buf, "\r\n");	// add trailing \r\n to communicate with the ESP
		AT_status = ESP8266_SendATCommandKeepString(conn->wifi->buf, RESPONSE_MAX_SIZE, AT_SHORT_TIMEOUT);
		memset(conn->wifi->buf, 0, RESPONSE_MAX_SIZE);

		if (AT_status == OK)
		{
		  sprintf(conn->wifi->buf, "DATA:\n");

		  char* esp_buffer = ESP8266_GetBuffer();
		  if (strlen(conn->wifi->buf) + strlen(esp_buffer) > RESPONSE_MAX_SIZE) return ERR;

		  strcat(conn->wifi->buf, ESP8266_GetBuffer());	// get ESP response
		  ESP8266_ClearBuffer();
		}
		else if (AT_status == ERR)
		  sprintf(conn->wifi->buf, "FROM ESP: ERROR");
		else if (AT_status == TIMEOUT)
		  sprintf(conn->wifi->buf, "ESP TIMEOUT");
		else
		  sprintf(conn->wifi->buf, "UNKNOWN ANSWER");

		AT_status = WIFI_SendResponse(conn, "200 OK", conn->wifi->buf, strlen(conn->wifi->buf));
	}


	return AT_status;
}

Response_t WIFI_HandleValveRequest(Connection_t* conn, Valve_t* valve1, char* key_ptr)
{
	// key_ptr = strstr(conn.request, "valve=")
	/*if (key_ptr - conn->request > strlen(conn->request))
		return WIFI_SendResponse(conn, "400 Bad Request", "ID non valido", 13);*/

	char* valve_id_ptr = WIFI_GetKeyValue(conn, key_ptr, NULL);

	if (valve_id_ptr == NULL)
		return WIFI_SendResponse(conn, "400 Bad Request", "ID non valido", 13);

	if (strstr(valve_id_ptr, "help"))
		return WIFI_SendResponse(conn, "200 OK", (char*)valve_help_message, sizeof(valve_help_message));

	uint32_t requested_valve_id = 0;
	sscanf(valve_id_ptr, "%" PRIu32, &requested_valve_id);

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

	char* cmd_key_ptr = NULL;
	if ((cmd_key_ptr = WIFI_RequestHasKey(conn, "cmd")) == NULL)
		return WIFI_SendResponse(conn, "400 Bad Request", "Comando non trovato", 19);

	uint32_t cmd_size = 0;
	char* cmd_ptr = WIFI_GetKeyValue(conn, cmd_key_ptr, &cmd_size);

	if (cmd_size > RESPONSE_MAX_SIZE)
		return WIFI_SendResponse(conn, "400 Bad Request", "Comando non valido", 19);

	memset(conn->wifi->buf, 0, WIFI_BUF_MAX_SIZE);
	memcpy(conn->wifi->buf, cmd_ptr, cmd_size);

	/*uint32_t cmd_index = cmd_ptr + 4 - conn->request;
	if (cmd_index > strlen(conn->request))
		return WIFI_SendResponse(conn, "400 Bad Request", "Comando non valido", 18);

	uint32_t cmd_size = strlen(conn->request) - (cmd_index);
	memset(conn->response_buffer, 0, RESPONSE_MAX_SIZE);
	memcpy(conn->response_buffer, conn->request + cmd_index, cmd_size);*/

	if (strstr(conn->wifi->buf, "open"))
	{
		VALVE_Open(valve);
		return WIFI_SendResponse(conn, "200 OK", "Valvola aperta", 14);
	}
	else if(strstr(conn->wifi->buf, "close"))
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

