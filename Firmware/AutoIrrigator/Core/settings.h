/*
 * settings.h
 *
 *  Created on: Aug 6, 2025
 *      Author: kikkiu
 */

#ifndef SETTINGS_H_
#define SETTINGS_H_

#include <inttypes.h>
#include "stm32g0xx.h"
#include "main.h"

typedef uint8_t bool;
#define true 1
#define false 0

// CHANGE THESE SETTINGS ACCORDING TO YOUR SETUP!!!
#define STM_UART					huart1
#define UART_DMA_CHANNEL_HANDLE		DMA1_Channel1
#define UART_DMA_LL_CHANNEL			LL_DMA_CHANNEL_1
#define UART_DMA_TYPEDEF			DMA1
#define ESP_RST_PORT				ESPRST_GPIO_Port
#define ESP_RST_PIN					ESPRST_Pin

#define STATUS_Port					STATUS_GPIO_Port
//#define STATUS_Pin					STATUS_Pin

// if START_ATTEMPTS is set to -1, the program won't start until it receives
// "ready" from the ESP (infinite retries)
#define START_ATTEMPTS -1

// ==========================================================================================
// 										USER DEFINES
// ==========================================================================================
#define MAX_SENSORS 10
#define MAX_SWITCHES 10
#define MAX_COVERS 2
#define MAX_LIGHTS 2
#define MAX_SERVOS 2

#define ENABLE_SERVO
#define SERVO_DEFAULT_MIN_ANGLE 0
#define SERVO_DEFAULT_MAX_ANGLE 180

#define OVERTEMP_THRESHOLD 85	// °C

// ==========================================================================================
// 											FLASH
// ==========================================================================================
/**
 *				!!!THE LAST PAGE OF THE FLASH MEMORY HAS TO BE BLANK!!!
 * 						!!!CHECK PROGRAM SIZE BEFORE UPLOADING!!!
 */
#define ENABLE_SAVE_TO_FLASH

#ifdef ENABLE_SAVE_TO_FLASH
// check your datasheet for the permitted datatype! STM32G030 can write DWORD on FLASH
typedef uint64_t FLASH_DATATYPE;
#define FLASH_DATASIZE sizeof(FLASH_DATATYPE)
#define LAST_PAGE_ADDRESS (0x08000000 + ((FLASH_PAGE_NB - 1) * FLASH_PAGE_SIZE))
#endif

// ==========================================================================================
// 									NETWORK (esp8266.h)
// ==========================================================================================
static const char RADIO_POWER[] = "65";
static const char ESP_NAME[] = "SNSE device";
static const char ESP_HOSTNAME[] = "SNSEDEVICE89AF"; // template: SNSEDEVICExxxx

// MQTT Broker configurations
#define MQTT_BROKER_IP          "192.168.1.2"
#define MQTT_BROKER_PORT        1883
#define MQTT_PUBLISH_INTERVAL   1000 // in ms

// AT command timeout configurations
#define AT_SHORT_TIMEOUT 250
#define AT_MEDIUM_TIMEOUT 500
#define AT_LONG_TIMEOUT 1250

// Buffer sizes in RAM (Adjust according to memory limits)
#define WIFI_BUF_MAX_SIZE 512
#define UART_BUFFER_SIZE 1536

#define HOSTNAME_MAX_SIZE 32
#define NAME_MAX_SIZE 32

#define UART_TX_TIMEOUT 500         // ms
#define UART_RX_IDLE_TIMEOUT 3000   // ms

#define RECONNECT_CHECK_INTERVAL 60000 // ms
#define RECONNECTION_DELAY_MINS 1    // minutes
#define RECONNECTION_DELAY_MILLIS (RECONNECTION_DELAY_MINS * 60000)

typedef struct
{
    char* text;
    uint8_t size;
    bool read;
    bool clear_if_read;
} Notification_t;

extern Notification_t notification;

// ==========================================================================================
// 									IRRIGATION / SCHEDULE
// ==========================================================================================
#define VALVES_NUM 4
#define MIN_WATER_FLOW 30			// liters per hour
#define SCHEDULE_TIME_SIZE 11		// hh:mm-dddd
#define DEFAULT_SCHEDULE "00:00-0000"

// ==========================================================================================
// 									IRRIGATION STRUCTS
// ==========================================================================================
typedef struct flow
{
	uint32_t 			last_ic_val;
	uint32_t 			ic_val;
	uint32_t 			ic_timestamp;
	uint32_t 			period_us;
	uint32_t 			lt_per_hour;
} Flow_t;

typedef struct schedule
{
	int8_t 				hour_open;
	int8_t 				minute_open;
	uint16_t			duration;
	char				text[SCHEDULE_TIME_SIZE+1];
} Schedule_t;

// Forward declaration of Switch_t
typedef struct Switch_s Switch_t;

typedef struct valve
{
	uint8_t 			id;
	uint8_t 			isOpen;
	GPIO_TypeDef* 		gpio_port;
	uint16_t 			gpio_pin;
	struct flow* 		flow;
	struct schedule* 	schedule;
	bool 				has_manual_override;
	Switch_t* 			sw;
} Valve_t;

typedef struct bat
{
	uint16_t voltage_mv;
	uint16_t voltage_integer;
	uint16_t voltage_decimal;
} Battery_t;

extern Battery_t bat;

// ==========================================================================================
// 											WEATHER
// ==========================================================================================
#define PROBABLE_PRECIPITATION 40			// %, PROB40
#define LOW_PROB_PRECIPITATION 30			// %, PROB30
#define PRECIPITATION_THRESHOLD	1			// millimeters

// ==========================================================================================
// 										SAVE DATA
// ==========================================================================================
#ifdef ENABLE_SAVE_TO_FLASH
typedef struct sdata
{
	char schedules[SCHEDULE_TIME_SIZE * VALVES_NUM];
	char name[NAME_MAX_SIZE + 1];
	char ip[15 + 1];
#ifdef ENABLE_SERVO
	uint8_t servo_min_angle[MAX_SERVOS];
	uint8_t servo_max_angle[MAX_SERVOS];
	uint8_t servo_angle[MAX_SERVOS];
#endif
} SaveData_t;

extern SaveData_t savedata;
#endif

static const char NOTIFICATION_WEATHER_NO_VALVE_OPEN[] = "Valvole bloccate per pioggia";

#endif /* SETTINGS_H_ */
