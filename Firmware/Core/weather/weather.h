/*
 * weather.h
 *
 *  Created on: Apr 25, 2025
 *      Author: Kikkiu
 */

#ifndef WEATHER_WEATHER_H_
#define WEATHER_WEATHER_H_

#include "../ESP8266/esp8266.h"
#include <stdio.h>
#include <string.h>

#define PROBABLE_PRECIPITATION 40			// %, PROB40
#define LOW_PROB_PRECIPITATION 30			// %, PROB30

typedef struct weather
{
	uint8_t last_update_status;				// update successful (1) or not (0)
	char forecast_date[10];					// yyyy-mm-dd
	char forecast_time[5];					// hh:mm
	uint32_t current_precipitation;			// microns
	uint8_t forecast_hour;					// h
	uint32_t hourly_precipitation[24];		// microns
	uint8_t hourly_precipitation_prob[24];	// %
} Weather_t;

uint8_t WEATHER_GetForecast(Weather_t* weather, char* uart_buffer);
int8_t WEATHER_GetProbablePrecipitation(Weather_t* wx, uint8_t start_hour);
int8_t WEATHER_GetLowProbPrecipitation(Weather_t* wx, uint8_t start_hour);

#endif /* WEATHER_WEATHER_H_ */
