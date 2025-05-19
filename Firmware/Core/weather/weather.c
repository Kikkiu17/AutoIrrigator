/*
 * weather.c
 *
 *  Created on: Apr 25, 2025
 *      Author: Kikkiu
 */

#include "weather.h"

const uint8_t WEATHER_START[] = "AT+CIPSTART=4,\"TCP\",\"api.open-meteo.com\",80\r\n";
const uint8_t WEATHER_SENDER[] = "AT+CIPSEND=4,227\r\n"; // EDIT THE MESSAGE LENGTH WHEN IT IS CHANGED!!!!!
const uint8_t WEATHER_DATA[] = "GET /v1/forecast?latitude=45.5582&longitude=9.2149&hourly=precipitation,precipitation_probability&current=precipitation&timezone=auto&forecast_days=1 HTTP/1.1"
	"\nAccept: text/plain"
	"\nHost: api.open-meteo.com"
	"\nConnection: keep-alive\n\n";
const uint8_t WEATHER_CLOSE[] = "AT+CIPCLOSE=4\r\n";

uint8_t WEATHER_GetAPIData()
{
	char* buf = ESP8266_GetBuffer();

	if (ESP8266_SendATCommandResponse((char*)WEATHER_START, sizeof(WEATHER_START), 1000) != OK)
	{
		HAL_UART_Transmit(&STM_UART, WEATHER_CLOSE, sizeof(WEATHER_CLOSE), 1000);
		return 0;
	}

	if (ESP8266_SendATCommandResponse((char*)WEATHER_SENDER, sizeof(WEATHER_SENDER), 1000) != OK)
	{
		HAL_UART_Transmit(&STM_UART, WEATHER_CLOSE, sizeof(WEATHER_CLOSE), 1000);
		return 0;
	}

	HAL_UART_Transmit(&STM_UART, WEATHER_DATA, sizeof(WEATHER_DATA), 1000);

	char* ipd_ptr = NULL;
	uint32_t timestamp = uwTick;
	while (ipd_ptr == NULL && uwTick - timestamp < AT_MEDIUM_TIMEOUT)
	{
		HAL_Delay(1);
		ipd_ptr = strstr(buf, "+IPD");
	}

	if (ipd_ptr == NULL)
	{
		HAL_UART_Transmit(&STM_UART, WEATHER_CLOSE, sizeof(WEATHER_CLOSE), 1000);
		return 0;
	}

	char* start_ptr = NULL;
	char* end_ptr = NULL;
	timestamp = uwTick;
	while (end_ptr == NULL && uwTick - timestamp < AT_MEDIUM_TIMEOUT)
	{
		HAL_Delay(1);
		if (start_ptr == NULL)
			start_ptr = strstr(ipd_ptr, "\r\n\r\n");
		else
			end_ptr = strstr(start_ptr + 4, "\r\n\r\n");
	}

	if (end_ptr == NULL)
	{
		HAL_UART_Transmit(&STM_UART, WEATHER_CLOSE, sizeof(WEATHER_CLOSE), 1000);
		return 0;
	}

	HAL_UART_Transmit(&STM_UART, WEATHER_CLOSE, sizeof(WEATHER_CLOSE), 1000);
	return 1;
}

uint8_t WEATHER_GetPrecipitation(Weather_t* weather, char* uart_buffer, char* time_ptr)
{
	// GET CURRENT PRECIPITATIONS
	char* prec_now_ptr = strstr(time_ptr, "precipitation\":");
	if (prec_now_ptr == NULL) return 0;
	prec_now_ptr += 15;
	char* prec_now_end_ptr = strstr(prec_now_ptr, "}");
	if (prec_now_end_ptr == NULL) return 0;
	prec_now_end_ptr -= 1;
	uint8_t prec_width = prec_now_end_ptr - prec_now_ptr + 1;
	uint32_t precipitation_now = 0;
	// get precipitation microns
	for (uint8_t i = 0; i < prec_width; i++)
	{
		if (prec_now_ptr[i] == '.') continue;
		precipitation_now *= 10;
		precipitation_now += prec_now_ptr[i] - '0';
	}
	weather->current_precipitation = precipitation_now * 10;	// microns

	// GET PRECIPITATIONS
	char* prec_ptr = strstr(uart_buffer, "precipitation\":[");	// GET HOURLY PREC
	if (prec_ptr == NULL) return 0;

	char* h_ptr = prec_ptr + 16;
	memset(weather->hourly_precipitation, 0, 24 * sizeof(uint32_t));

	// get all the 24 hour precipitation probabilities
	// sscanf is trash (ads 2 kb to flash) so I do this manually
	for (uint8_t h = 0; h < 24; h++)
	{
		char* n_end_p = strstr(h_ptr, ",") - 1;
		char* arr_end_p = strstr(h_ptr, "]");
		char* resp_end_p = strstr(h_ptr, "\r\n");
		if (n_end_p > resp_end_p || n_end_p == arr_end_p)
			n_end_p = strstr(h_ptr, "]") - 1;
		uint32_t n = 0;
		uint32_t n_width = n_end_p - h_ptr + 1;
		for (uint8_t w = 0; w < n_width; w++)
		{
			if (*h_ptr == '.')
			{
				h_ptr++;
				continue;
			}
			n *= 10;
			n += *(h_ptr) - '0';
			h_ptr++;
		}
		weather->hourly_precipitation[h] = n * 10; // microns

		//												 v
		// h_ptr here points to the comma. example: ...12,23,27
		h_ptr++;
	}

	return 1;
}

uint8_t WEATHER_GetPrecipitationProbability(Weather_t* weather, char* uart_buffer)
{
	// GET PRECIPITATIONS PROB
	char* prob_ptr = strstr(uart_buffer, "precipitation_probability\":[");	// GET HOURLY PROB
	if (prob_ptr == NULL) return 0;

	char* h_ptr = prob_ptr + 28;
	memset(weather->hourly_precipitation_prob, 0, 24);

	// get all the 24 hour precipitation probabilities
	// sscanf is trash (ads 2 kb to flash) so I do this manually
	for (uint8_t h = 0; h < 24; h++)
	{
		char* n_end_p = strstr(h_ptr, ",") - 1;
		char* resp_end_p = strstr(h_ptr, "\r\n");
		if (n_end_p > resp_end_p)
			n_end_p = strstr(h_ptr, "]") - 1;
		uint8_t n = 0;
		uint8_t n_width = n_end_p - h_ptr + 1;
		for (uint8_t w = 0; w < n_width; w++)
		{
			n *= 10;
			n += *(h_ptr) - '0';
			h_ptr++;
		}
		weather->hourly_precipitation_prob[h] = n; // microns

		//												 v
		// h_ptr here points to the comma. example: ...12,23,27
		h_ptr++;
	}

	return 1;
}

uint8_t WEATHER_GetForecast(Weather_t* weather, char* uart_buffer)
{
	if (!WEATHER_GetAPIData()) return 0;

	//			  		v
	//current":{"time":"2025-04-25T00:00
	char* date_ptr = strstr(uart_buffer, "current\":{\"time\":\"");
	if (date_ptr == NULL) return 0;
	date_ptr += 18;

	// GET DATE
	//			v
	// 2025-04-25T00:00
	char* date_end_ptr = strstr(date_ptr, "T") - 1;
	//char* date_end_ptr = strstr(time_ptr + 18, "\"") - 7;
	uint32_t date_size = date_end_ptr - date_ptr + 1;
	if (date_size > sizeof(weather->forecast_date)) return 0;
	memcpy(weather->forecast_date, date_ptr, date_size); // DATE

	// GET TIME
	//			  v
	// 2025-04-25T00:00
	char* time_ptr = date_end_ptr + 2;
	memcpy(weather->forecast_time, time_ptr, sizeof(weather->forecast_time));
	weather->forecast_hour = bufferToInt(weather->forecast_time, 2);

	WEATHER_GetPrecipitation(weather, uart_buffer, time_ptr);
	WEATHER_GetPrecipitationProbability(weather, uart_buffer);

	ESP8266_ClearBuffer();

	weather->last_update_status = 1;

	return 1;
}

int8_t WEATHER_GetProbablePrecipitation(Weather_t* wx, uint8_t start_hour)
{
	for (uint8_t hour = start_hour; hour < 24; hour++)
	{
		uint8_t h_precipitation = wx->hourly_precipitation_prob[hour];
		if (h_precipitation >= PROBABLE_PRECIPITATION)
			return hour;
	}
	return -1;
}

int8_t WEATHER_GetLowProbPrecipitation(Weather_t* wx, uint8_t start_hour)
{
	for (uint8_t hour = start_hour; hour < 24; hour++)
	{
		uint8_t h_precipitation = wx->hourly_precipitation_prob[hour];
		if (h_precipitation >= LOW_PROB_PRECIPITATION)
			return hour;
	}
	return -1;
}
