/*
 * wifi.c
 *
 *  Created on: Apr 12, 2025
 *      Author: Kikkiu
 */

#include "wifihandler.h"

const char get_help_message[] =
{
		"Comandi GET:"
		"\nvalve"
		"\nwifi"
		"\nhelp"
};

const char post_help_message[] =
{
		"Comandi POST:"
		"\nat"
};

Response_t WIFIHANDLER_HandleWiFiRequest(WIFI_t* wifi, Connection_t* conn, char* command_ptr)
{
	Response_t atstatus = ERR;
	char* value_ptr = NULL;

	if ((value_ptr = strstr(conn->request, "getssid")) != NULL)
	{
		atstatus = WIFI_SendResponse(conn, "200 OK", wifi->SSID, sizeof(wifi->SSID));
	}
	else if ((value_ptr = strstr(conn->request, "getip")) != NULL)
	{
		atstatus = WIFI_SendResponse(conn, "200 OK", wifi->IP, sizeof(wifi->IP));
	}
	else if ((value_ptr = strstr(conn->request, "getbuf")) != NULL)
	{
		// this is possible if RESPONSE_MAX_SIZE is at least as big as WIFI_BUF_MAX_SIZE
		atstatus = WIFI_SendResponse(conn, "200 OK", wifi->buf, sizeof(wifi->buf));
	}
	else if ((value_ptr = strstr(conn->request, "help")) != NULL)
	{
		atstatus = WIFI_SendResponse(conn, "200 OK", "Comandi GET WiFi:\ngetssid\ngetip\ngetbuf\nhelp", 44);
	}
	else atstatus = WIFI_SendResponse(conn, "404 Not Found", "Comando WiFi non riconosciuto."
			"Scrivi wifi=help per una lista di comandi", 72);

	return atstatus;
}

Response_t WIFIHANDLER_HandleHelpRequest(Connection_t* conn)
{
	if (conn->request_type == GET)
		return WIFI_SendResponse(conn, "200 OK", (char*)get_help_message, sizeof(get_help_message));
	else if (conn->request_type == POST)
		return WIFI_SendResponse(conn, "200 OK", (char*)post_help_message, sizeof(post_help_message));
	else return WIFI_SendResponse(conn, "400 Bad Request", "Sono supportate solo richieste POST e GET", sizeof(41));
}
