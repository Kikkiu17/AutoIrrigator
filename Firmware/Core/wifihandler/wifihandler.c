/*
 * wifi.c
 *
 *  Created on: Apr 12, 2025
 *      Author: Kikkiu
 */

#include "wifihandler.h"

const char GET_HELP_MESSAGE[] =
{
		"Comandi GET:"
		"\n- valve"
		"\n- wifi"
		"\n- features"
		"\n- weather"
		"\n- help"
		"\n\nper ottenere aiuto per ogni comando, fai <comando>=help"
};

const char POST_HELP_MESSAGE[] =
{
		"Comandi POST:"
		"\n- at"
		"\n\nper ottenere aiuto per ogni comando, fai <comando>=help"
};

const char WIFI_HELP_MESSAGE[] =
{
		"Comandi GET WiFi:"
		"\n- SSID"
		"\n- IP"
		"\n- ID (ESPDEVICExxx)"
		"\n- name"
		"\n- buf"
		"\n- conn (dettagli connesione)"
		"\n- help"
		"\n\nComandi POST WiFi:"
		"\n- changename"
		"\n- help"
};

const char WEATHER_HELP_MESSAGE[] =
{
		"Comandi GET weather:"
		"\n- lowprob (prima ora in cui ci sara' probabilita' di pioggia maggiore del 30%)"
		"\n- prob (prima ora in cui ci sara' probabilita' di pioggia maggiore del 40%)"
		"\n- now (precipitazioni attuali)"
		"\n- precipitation (precipitazioni per ogni ora del giorno)"
		"\n- hourprob (prob di precipitazioni per ogni ora del giorno)"
		"\n- updatetime"
		"\n- help"
};

Response_t WIFIHANDLER_HandleWiFiRequest(Connection_t* conn, char* command_ptr)
{
	if (WIFI_RequestKeyHasValue(conn, command_ptr, "help"))
		return WIFI_SendResponse(conn, "200 OK", (char*)WIFI_HELP_MESSAGE, sizeof(WIFI_HELP_MESSAGE));

	if (conn->request_type == POST)
	{
		if (WIFI_RequestKeyHasValue(conn, command_ptr, "changename"))
		{
			char* name_ptr = WIFI_RequestHasKey(conn, "name");
			if (name_ptr == NULL)
				return WIFI_SendResponse(conn, "400 Bad Request", "Chiave \"name\" non trovata", 25);
			else
			{
				uint32_t name_size = 0;
				name_ptr = WIFI_GetKeyValue(conn, name_ptr, &name_size);
				if (name_ptr == NULL)
					return WIFI_SendResponse(conn, "400 Bad Request", "Nome non trovato", 16);
				else
				{
					WIFI_SetName(conn->wifi, name_ptr);
					return WIFI_SendResponse(conn, "200 OK", "Nome cambiato", 13);
				}
			}

		}
		else return WIFI_SendResponse(conn, "400 Bad Request", "Comando POST WiFi non riconosciuto. "
			"Scrivi wifi=help per una lista di comandi", 77);
	}
	else if (conn->request_type == GET)
	{
		if (WIFI_RequestKeyHasValue(conn, command_ptr, "SSID"))
		{
			return WIFI_SendResponse(conn, "200 OK", conn->wifi->SSID, strlen(conn->wifi->SSID));
		}
		else if (WIFI_RequestKeyHasValue(conn, command_ptr, "IP"))
		{
			return WIFI_SendResponse(conn, "200 OK", conn->wifi->IP, strlen(conn->wifi->IP));
		}
		else if (WIFI_RequestKeyHasValue(conn, command_ptr, "ID"))
		{
			return WIFI_SendResponse(conn, "200 OK", conn->wifi->hostname, strlen(conn->wifi->hostname));
		}
		else if (WIFI_RequestKeyHasValue(conn, command_ptr, "name"))
		{
			return WIFI_SendResponse(conn, "200 OK", conn->wifi->name, strlen(conn->wifi->name));
		}
		else if (WIFI_RequestKeyHasValue(conn, command_ptr, "buf"))
		{
			// this is possible if RESPONSE_MAX_SIZE is at least as big as WIFI_BUF_MAX_SIZE
			if (RESPONSE_MAX_SIZE < WIFI_BUF_MAX_SIZE)
			{
				return WIFI_SendResponse(conn, "500 Internal server error", "RESPONSE_MAX_SIZE is "
						"smaller than WIFI_BUF_MAX_SIZE", 51);
			}
			else
				return WIFI_SendResponse(conn, "200 OK", conn->wifi->buf, sizeof(conn->wifi->buf));
		}
		else if (WIFI_RequestKeyHasValue(conn, command_ptr, "conn"))
		{
			sprintf(conn->wifi->buf, "Connection ID: %d\nrequest size: %" PRIu32
					"\nrequest: %s", conn->connection_number, conn->request_size, conn->request);
			return WIFI_SendResponse(conn, "200 OK", conn->wifi->buf, strlen(conn->wifi->buf));
		}
		else return WIFI_SendResponse(conn, "400 Bad Request", "Comando GET WiFi non riconosciuto. "
				"Scrivi wifi=help per una lista di comandi", 76);
	}

	return ERR;
}

Response_t WIFIHANDLER_HandleHelpRequest(Connection_t* conn)
{
	if (conn->request_type == GET)
		return WIFI_SendResponse(conn, "200 OK", (char*)GET_HELP_MESSAGE, sizeof(GET_HELP_MESSAGE));
	else if (conn->request_type == POST)
		return WIFI_SendResponse(conn, "200 OK", (char*)POST_HELP_MESSAGE, sizeof(POST_HELP_MESSAGE));
	else return WIFI_SendResponse(conn, "400 Bad Request", "Sono supportate solo richieste POST e GET", 41);
}

Response_t WIFIHANDLER_HandleFeaturePacket(Connection_t* conn, Valve_t* valve_list, uint32_t list_size, char* features_template)
{
	memset(conn->wifi->buf, 0, WIFI_BUF_MAX_SIZE);
	sprintf(conn->wifi->buf, features_template,
			valve_list[0].isOpen, valve_list[0].flow->lt_per_hour,
			valve_list[1].isOpen, valve_list[1].flow->lt_per_hour,
			valve_list[2].isOpen, valve_list[2].flow->lt_per_hour,
			valve_list[3].isOpen, valve_list[3].flow->lt_per_hour,
			valve_list[0].schedule->text,
			valve_list[1].schedule->text,
			valve_list[2].schedule->text,
			valve_list[3].schedule->text,
			uwTick);
	return WIFI_SendResponse(conn, "200 OK", conn->wifi->buf, strlen(conn->wifi->buf));
}

Response_t WIFIHANDLER_HandleWeatherRequest(Weather_t* wx, Connection_t* conn, char* key_ptr)
{
	char* wbuf = conn->wifi->buf;

	if (WIFI_RequestKeyHasValue(conn, key_ptr, "help"))
		return WIFI_SendResponse(conn, "200 OK", (char*)WEATHER_HELP_MESSAGE, sizeof(WEATHER_HELP_MESSAGE));

	if (!wx->last_update_status)
	{
		wx->last_update_status = WEATHER_GetForecast(wx, ESP8266_GetBuffer());
		if (!wx->last_update_status)
			return WIFI_SendResponse(conn, "500 Internal Server Error", "Impossibile ottenere le previsioni", 34);
	}

	if (WIFI_RequestKeyHasValue(conn, key_ptr, "lowprob"))
	{
		int8_t hour = WEATHER_GetLowProbPrecipitation(wx, 0);
		if (hour == -1)
			return WIFI_SendResponse(conn, "200 OK", "Oggi la probabilita' di pioggia"
					" e' minore del 30% in tutte le ore", 65);
		else
		{
			memset(wbuf, 0, WIFI_BUF_MAX_SIZE);
			sprintf(wbuf, "La prima ora in cui ci sara' probabilita' di pioggia maggiore"
					" del 30%% saranno le %d", hour);
			return WIFI_SendResponse(conn, "200 OK", wbuf, strlen(wbuf));
		}
	}
	else if (WIFI_RequestKeyHasValue(conn, key_ptr, "prob"))
	{
		int8_t hour = WEATHER_GetProbablePrecipitation(wx, 0);
		if (hour == -1)
			return WIFI_SendResponse(conn, "200 OK", "Oggi la probabilita' di pioggia"
					" e' minore del 40% in tutte le ore", 65);
		else
		{
			memset(wbuf, 0, WIFI_BUF_MAX_SIZE);
			sprintf(wbuf, "La prima ora in cui ci sara' probabilita' di pioggia maggiore"
					" del 40%% saranno le %d", hour);
			return WIFI_SendResponse(conn, "200 OK", wbuf, strlen(wbuf));
		}
	}
	else if (WIFI_RequestKeyHasValue(conn, key_ptr, "now"))
	{
		if (wx->current_precipitation == 0)
			return WIFI_SendResponse(conn, "200 OK", "Non sta piovendo", 16);
		else
		{
			memset(wbuf, 0, WIFI_BUF_MAX_SIZE);
			sprintf(wbuf, "Ci sono %" PRIu32 " mm di precipitazioni", wx->current_precipitation);
			return WIFI_SendResponse(conn, "200 OK", wbuf, strlen(wbuf));
		}
	}
	else if (WIFI_RequestKeyHasValue(conn, key_ptr, "precipitation"))
	{
		memset(wbuf, 0, WIFI_BUF_MAX_SIZE);
		sprintf(wbuf, "Precipitazioni di oggi per ogni ora: ");
		char prec[24];
		for (uint8_t i = 0; i < 24; i++)
		{
			uint32_t int_prec = wx->hourly_precipitation[i] / 1000;
			uint32_t dec_prec = wx->hourly_precipitation[i] - int_prec * 1000;
			if (dec_prec > 99)
				dec_prec /= 10;
			sprintf(prec, "%" PRIu32 ".%" PRIu32, int_prec, dec_prec);
			strcat(wbuf, prec);
			if (i != 23)
				strcat(wbuf, ",");
		}

		WIFI_SendResponse(conn, "200 OK", wbuf, strlen(wbuf));
	}
	else if (WIFI_RequestKeyHasValue(conn, key_ptr, "hourprob"))
	{
		memset(wbuf, 0, WIFI_BUF_MAX_SIZE);
		sprintf(wbuf, "Probabilita' di precipitazioni per ogni ora di oggi: ");
		char prob[24];
		for (uint8_t i = 0; i < 24; i++)
		{
			sprintf(prob, "%d", wx->hourly_precipitation_prob[i]);
			strcat(wbuf, prob);
			if (i != 23)
				strcat(wbuf, ",");
		}

		WIFI_SendResponse(conn, "200 OK", wbuf, strlen(wbuf));
	}
	else if (WIFI_RequestKeyHasValue(conn, key_ptr, "updatetime"))
	{
		WIFI_SendResponse(conn, "200 OK", wx->forecast_time, sizeof(wx->forecast_time));
	}

	return WIFI_SendResponse(conn, "400 Bad Request", "Tipo di richiesta non trovato", 29);

	return ERR;
}
