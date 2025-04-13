/*
 * wifi.h
 *
 *  Created on: Apr 12, 2025
 *      Author: Kikkiu
 */

#ifndef WIFI_WIFIHANDLER_H_
#define WIFI_WIFIHANDLER_H_

#include "../ESP8266/esp8266.h"
#include <string.h>

extern const char get_help_message[];

extern const char post_help_message[];

Response_t WIFIHANDLER_HandleWiFiRequest(Connection_t* conn, char* command_ptr);
Response_t WIFIHANDLER_HandleHelpRequest(Connection_t* conn);

#endif /* WIFI_WIFIHANDLER_H_ */
