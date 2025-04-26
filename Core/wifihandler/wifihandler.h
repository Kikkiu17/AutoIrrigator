/*
 * wifi.h
 *
 *  Created on: Apr 12, 2025
 *      Author: Kikkiu
 */

#ifndef WIFI_WIFIHANDLER_H_
#define WIFI_WIFIHANDLER_H_

#include <string.h>
#include <inttypes.h>
#include <stdio.h>

#include "../ESP8266/esp8266.h"
#include "../irrigator/irrigator.h"
#include "../weather/weather.h"

extern const char get_help_message[];

extern const char post_help_message[];

Response_t WIFIHANDLER_HandleWiFiRequest(Connection_t* conn, char* command_ptr);
Response_t WIFIHANDLER_HandleHelpRequest(Connection_t* conn);
Response_t WIFIHANDLER_HandleFeaturePacket(Connection_t* conn, Valve_t* valve_list, uint32_t list_size, char* features_template);
Response_t WIFIHANDLER_HandleWeatherRequest(Weather_t* wx, Connection_t* conn, char* key_ptr);

#endif /* WIFI_WIFIHANDLER_H_ */
