/*
 * wifihandler.h
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
#include "../settings.h"

typedef struct Switch_s
{
    const char* switch_id;  // unique ID string (e.g. "relay1", "light_main")
    const char* name;       // friendly name for HA (e.g. "Main Relay")
    bool pressed;           // current state
    bool inverted;          // active low switch if true
    GPIO_TypeDef* port;     // STM32 GPIO Port
    uint16_t pin;           // STM32 GPIO Pin
    bool manual;            // manual override flag
} Switch_t;

typedef struct
{
    const char* name;           // e.g. "Voltage sensor"
    const char* unit;           // e.g. "V"
    const char* sensor_id;      // e.g. "supply_volt"
    const char* sensor_type;    // e.g. "voltage"
    int32_t* value_ptr;         // pointer to scaled sensor value (e.g. 1234 for 12.34V)
    uint8_t decimals;           // number of decimal places (0 for ints, e.g. 2 for 0.01 resolution)
} Sensor_t;

#ifdef ENABLE_COVER
typedef struct
{
    const char* cover_id;           // unique ID string (e.g. "cover1", "shutter")
    const char* friendly_name;      // friendly name for HA
    STEPPER_DRIVER_HANDLE* stepper; // pointer to stepper handle (change in settings.h)
    StepperState_t last_state;      // needed to report the stop position to HA
} Cover_t;
#endif

#ifdef ENABLE_LIGHT
typedef struct
{
    const char* light_id;           // unique ID string (e.g. "light1")
    const char* friendly_name;      // friendly name for HA
    
    TIM_HandleTypeDef* htim;        // STM32 Timer handle (e.g. &htim14)
    uint32_t channel;               // timer channel (e.g. TIM_CHANNEL_1)
    const uint16_t* gamma_table;    // pointer to 256-element gamma lookup table (NULL if linear)
    bool inverted;                  // true = active low(inverted), false = active high (normal)
    
    bool state;
    uint8_t current_brightness;
    uint8_t target_brightness;      // target level requested by HA
    uint32_t last_step_time;        // for brightness fade
    uint16_t fade_step_delay_ms;    // speed of fade (default 4ms per step so ~1 sec max fade)
} Light_t;
#endif

#ifdef ENABLE_SERVO
#include "servo.h"

extern Servo_t servos[MAX_SERVOS];
extern uint32_t servo_count;
#endif

extern Switch_t switches[MAX_SWITCHES];

void NOTIFICATION_Reset(void);
void NOTIFICATION_Set(char* text, uint8_t size);

Response_t WIFIHANDLER_ReconnectIfDisconnected(WIFI_t *wifi);

// ====================================================================
// SENSOR FUNCTIONS
// ====================================================================
bool WIFIHANDLER_MQTT_RegisterSensor(const char* friendly_name, const char* unit,
    const char* sensor_id, const char* sensor_type, int32_t* value_ptr, uint8_t decimals);
void WIFIHANDLER_MQTT_PubDiscoverySensor(WIFI_t* wifi, Sensor_t* sensor);
void WIFIHANDLER_MQTT_PubStateSensor(WIFI_t* wifi, Sensor_t* sensor);
void WIFIHANDLER_MQTT_PubStateSensorByID(WIFI_t* wifi, const char* sensor_id);

// ====================================================================
// SWITCH FUNCTIONS
// ====================================================================
bool WIFIHANDLER_MQTT_RegisterSwitch(const char* switch_id, const char* name, bool inverted, GPIO_TypeDef* port, uint16_t pin, bool default_on);
void WIFIHANDLER_MQTT_PubDiscoverySwitch(WIFI_t* wifi, Switch_t* sw);
void WIFIHANDLER_MQTT_PubStateSwitch(WIFI_t* wifi, Switch_t* sw);
void WIFIHANDLER_MQTT_PubStateSwitchByID(WIFI_t* wifi, const char* switch_id);

void SWITCH_Init(Switch_t* sw, const char* switch_id, const char* name, bool inverted, GPIO_TypeDef* port, uint16_t pin);
void SWITCH_Press(Switch_t* sw);
void SWITCH_UnPress(Switch_t* sw);
Switch_t* SWITCH_GetByID(const char* switch_id);

// ====================================================================
// COVER FUNCTIONS
// ====================================================================
#ifdef ENABLE_COVER
bool WIFIHANDLER_MQTT_RegisterCover(const char* cover_id, const char* friendly_name, STEPPER_DRIVER_HANDLE* stepper);
Cover_t* COVER_GetByID(const char* cover_id);

void COVER_Open(Cover_t* cover);
void COVER_Close(Cover_t* cover);
void COVER_Stop(Cover_t* cover);
void COVER_SetPosition(Cover_t* cover, uint8_t percent);

void WIFIHANDLER_MQTT_PubDiscoveryCover(WIFI_t* wifi, Cover_t* cover);
void WIFIHANDLER_MQTT_PubStateCover(WIFI_t* wifi, Cover_t* cover);
void WIFIHANDLER_MQTT_PubStateCoverByID(WIFI_t* wifi, const char* cover_id);
#endif

// ====================================================================
// LIGHT FUNCTIONS
// ====================================================================
#ifdef ENABLE_LIGHT
bool WIFIHANDLER_MQTT_RegisterLight(const char* light_id, const char* friendly_name, 
    TIM_HandleTypeDef* htim, uint32_t channel,  const uint16_t* gamma_table, bool inverted);

Light_t* LIGHT_GetByID(const char* light_id);

void LIGHT_SetState(Light_t* light, bool state);
void LIGHT_SetBrightness(Light_t* light, uint8_t brightness);
void LIGHT_UpdatePWM(Light_t* light);
void LIGHT_ProcessFade(Light_t* light);

void WIFIHANDLER_MQTT_PubDiscoveryLight(WIFI_t* wifi, Light_t* light);
void WIFIHANDLER_MQTT_PubStateLight(WIFI_t* wifi, Light_t* light);
void WIFIHANDLER_MQTT_PubStateLightByID(WIFI_t* wifi, const char* light_id);
#endif

// ====================================================================
// SERVO FUNCTIONS
// ====================================================================
#ifdef ENABLE_SERVO
bool WIFIHANDLER_MQTT_RegisterServo(const char* servo_id, const char* friendly_name, 
    TIM_HandleTypeDef* htim, uint32_t channel, uint8_t min_angle, uint8_t max_angle, bool inverted);

Servo_t* SERVO_GetByID(const char* servo_id);

void WIFIHANDLER_MQTT_PubDiscoveryServo(WIFI_t* wifi, Servo_t* servo);
void WIFIHANDLER_MQTT_PubStateServo(WIFI_t* wifi, Servo_t* servo);
void WIFIHANDLER_MQTT_PubStateServoByID(WIFI_t* wifi, const char* servo_id);
#endif

// ====================================================================
// MQTT FUNCTIONS
// ====================================================================
Response_t WIFIHANDLER_MQTT_Init(WIFI_t* wifi, const char* broker_ip, uint16_t port);
void WIFIHANDLER_MQTT_PublishAllDiscovery(WIFI_t* wifi);
void WIFIHANDLER_MQTT_PublishAllStates(WIFI_t* wifi);
void WIFIHANDLER_MQTT_Loop(WIFI_t* wifi);
void WIFIHANDLER_MQTT_SendNotification(WIFI_t* wifi, const char* message);

#endif /* WIFI_WIFIHANDLER_H_ */
