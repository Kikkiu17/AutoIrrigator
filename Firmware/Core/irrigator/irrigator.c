/*
 * valve.c
 *
 *  Created on: Apr 7, 2025
 *      Author: Kikkiu
 */

#include "irrigator.h"

#include <stdio.h>
#include <inttypes.h>
#include "usart.h"

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

	memset(conn->wifi->buf, 0, WIFI_BUF_MAX_SIZE);
	memcpy(conn->wifi->buf, command, command_size);

	if (strstr(conn->wifi->buf, "\\r") || strstr(conn->wifi->buf, "\\n"))
	{
		AT_status = WIFI_SendResponse(conn, "400 Bad Request", "Non inviare i caratteri \\r e \\n", 31);
		memset(conn->wifi->buf, 0, WIFI_BUF_MAX_SIZE);
	}
	else
	{
		strcat(conn->wifi->buf, "\r\n");	// add trailing \r\n to communicate with the ESP
		AT_status = ESP8266_SendATCommandKeepString(conn->wifi->buf, strlen(conn->wifi->buf), AT_SHORT_TIMEOUT);
		memset(conn->wifi->buf, 0, WIFI_BUF_MAX_SIZE);

		if (AT_status == OK)
		{
		  sprintf(conn->wifi->buf, "DATA:\n");

		  char* esp_buffer = ESP8266_GetBuffer();
		  if (strlen(conn->wifi->buf) + strlen(esp_buffer) > WIFI_BUF_MAX_SIZE) return ERR;

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

Response_t WIFIHANDLER_HandleValveRequest(Connection_t* conn, Valve_t* valve_list, uint32_t list_size, char* key_ptr)
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

	Valve_t* valve = NULL;
	for (uint8_t i = 0; i < list_size; i++)
	{
		if (valve_list[i].id == requested_valve_id)
		{
			valve = &(valve_list[i]);
			break;
		}
	}

	if (valve == NULL)
		return WIFI_SendResponse(conn, "400 Bad Request", "ID non trovato", 14);

	if (conn->request_type == GET)
	{
		memset(conn->wifi->buf, 0, WIFI_BUF_MAX_SIZE);
		sprintf(conn->wifi->buf, "%s", valve->status);
		return WIFI_SendResponse(conn, "200 OK", conn->wifi->buf, strlen(conn->wifi->buf));
	}

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

	if (WIFI_RequestKeyHasValue(conn, cmd_key_ptr, "open"))
	{
		VALVE_Open(valve);
		return WIFI_SendResponse(conn, "200 OK", "Valvola aperta", 14);
	}
	else if(WIFI_RequestKeyHasValue(conn, cmd_key_ptr, "close"))
	{
		VALVE_Close(valve);
		return WIFI_SendResponse(conn, "200 OK", "Valvola chiusa", 14);
	}
	else
		return WIFI_SendResponse(conn, "400 Bad Request", "Comando non riconosciuto", 24);
}

void VALVE_Init(Valve_t* valve, Flow_t* flow, uint8_t id, GPIO_TypeDef* valve_port, uint16_t valve_pin)
{
	valve->flow = flow;
	valve->id = id;
	memcpy(valve->status, "chiusa", 6);
	valve->gpio_port = valve_port;
	valve->gpio_pin = valve_pin;
}

void VALVE_Open(Valve_t* valve)
{
	memcpy(valve->status, "aperta", 6);
	HAL_GPIO_WritePin(valve->gpio_port, valve->gpio_pin, 1);
}

void VALVE_Close(Valve_t* valve)
{
	memcpy(valve->status, "chiusa", 6);
	HAL_GPIO_WritePin(valve->gpio_port, valve->gpio_pin, 0);
}

void FLOW_CalculateFlow(Flow_t* flow)
{
	if (flow->ic_val > flow->last_ic_val)
	{
		flow->ic_timestamp = uwTick;
		flow->period_us = (flow->ic_val - flow->last_ic_val) * 8; // PSC is 255
		//flow->lt_per_hour = 1.0 / ((float)flow->period_us / 1000000.0) / 7.5 * 60.0;
		flow->lt_per_hour = 8000000 / flow->period_us;
	}
	flow->last_ic_val = flow->ic_val;
	if (flow->lt_per_hour < MIN_WATER_FLOW)
		flow->lt_per_hour = 0;
}
