#ifndef __USERHANDLERS_H__
#define __USERHANDLERS_H__

#include <string.h>
#include <inttypes.h>
#include <stdio.h>

#include "../ESP8266/esp8266.h"
#include "../settings.h"
#include "wifihandler.h"

extern int32_t flow1_val, flow2_val, flow3_val, total_flow_val, bat_voltage_val;
extern bool rain_factor_active;

// Custom user handler prototypes
void USER_CalculateSensors(void);
void USER_PublishSensors(WIFI_t* wifi);
void USER_MQTT_PublishRainFactor(WIFI_t* wifi, bool is_factor);

// MQTT Custom Hooks
void USER_MQTT_CustomInit(WIFI_t* wifi);
void USER_MQTT_CustomDiscovery(WIFI_t* wifi);
void USER_MQTT_CustomPublishStates(WIFI_t* wifi);
void USER_MQTT_CustomHandler(WIFI_t* wifi, const char* topic, const char* payload);

#endif /* __USERHANDLERS_H__ */
