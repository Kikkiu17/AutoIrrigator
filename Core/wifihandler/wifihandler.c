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
		"\n\nper ottenere aiuto per ogni comando, fai <comando>=help"
};

const char post_help_message[] =
{
		"Comandi POST:"
		"\nat"
		"\n\nper ottenere aiuto per ogni comando, fai <comando>=help"
};

Response_t WIFIHANDLER_HandleWiFiRequest(Connection_t* conn, char* command_ptr)
{
	Response_t atstatus = ERR;

	if (WIFI_RequestKeyHasValue(conn, command_ptr, "getssid"))
	{
		atstatus = WIFI_SendResponse(conn, "200 OK", conn->wifi->SSID, sizeof(conn->wifi->SSID));
	}
	else if (WIFI_RequestKeyHasValue(conn, command_ptr, "getip"))
	{
		atstatus = WIFI_SendResponse(conn, "200 OK", conn->wifi->IP, sizeof(conn->wifi->IP));
	}
	else if (WIFI_RequestKeyHasValue(conn, command_ptr, "getbuf"))
	{
		// this is possible if RESPONSE_MAX_SIZE is at least as big as WIFI_BUF_MAX_SIZE
		if (RESPONSE_MAX_SIZE < WIFI_BUF_MAX_SIZE)
		{
			atstatus = WIFI_SendResponse(conn, "500 Internal server error", "RESPONSE_MAX_SIZE is "
					"smaller than WIFI_BUF_MAX_SIZE", 51);
		}
		else
			atstatus = WIFI_SendResponse(conn, "200 OK", conn->wifi->buf, sizeof(conn->wifi->buf));
	}
	else if (WIFI_RequestKeyHasValue(conn, command_ptr, "help"))
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
