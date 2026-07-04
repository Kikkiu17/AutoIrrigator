/*
 * wifihandler.h
 *
 * Modified for: MQTT Client
 */

#ifndef WIFI_WIFIHANDLER_H_
#define WIFI_WIFIHANDLER_H_

#include <string.h>
#include <inttypes.h>
#include <stdio.h>

#include "../ESP8266/esp8266.h"
#include "../irrigator/irrigator.h"
#include "../settings.h"

Response_t WIFIHANDLER_MQTT_Init(WIFI_t* wifi, const char* broker_ip, uint16_t port);
void WIFIHANDLER_MQTT_PublishDiscovery(WIFI_t* wifi);
void WIFIHANDLER_MQTT_PublishStates(WIFI_t* wifi, Valve_t* valve_list);
void WIFIHANDLER_MQTT_Loop(WIFI_t* wifi, Valve_t* valve_list);
void WIFIHANDLER_MQTT_SendNotification(WIFI_t* wifi, const char* message);
void NOTIFICATION_Set(char* text, uint8_t size);
void NOTIFICATION_Reset(void);

#define RECONNECT_CHECK_INTERVAL 60000 // in milliseconds
Response_t WIFIHANDLER_ReconnectIfDisconnected(WIFI_t *wifi);

#endif /* WIFI_WIFIHANDLER_H_ */
