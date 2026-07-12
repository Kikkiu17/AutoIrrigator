/*
 * features.h
 *
 *  Created on: Aug 6, 2025
 *      Author: kikkiu
 */

#ifndef SETTINGS_H_
#define SETTINGS_H_

#include <inttypes.h>
#include "stm32g0xx.h"

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
#define LAST_PAGE_ADDRESS 0x08000000 + ((FLASH_PAGE_NB - 1) * FLASH_PAGE_SIZE)
//#define LAST_PAGE_ADDRESS 0x08000000 + FLASH_BANK_SIZE - FLASH_PAGE_SIZE
//#define LAST_PAGE_ADDRESS 0x8007800	// check your datasheet!!!
#endif

// ==========================================================================================
// 									NETWORK (esp8266.h)
// ==========================================================================================
static const char RADIO_POWER[] = "65";

static const char ESP_NAME[] = "SNSE device";

static const char ESP_HOSTNAME[] = "SNSEDEVICE89AF"; // template: SNSEDEVICExx
static const char MQTT_BROKER_IP[] = "192.168.1.2";
static const uint16_t MQTT_BROKER_PORT = 1883;
static const uint16_t MQTT_PUBLISH_INTERVAL = 1000;	// ms

#define AT_SHORT_TIMEOUT 250
#define AT_MEDIUM_TIMEOUT 500
#define AT_LONG_TIMEOUT 1250

// BUFFERS SIZES (in RAM)

/**
 * RESPONSE_MAX_SIZE
 *
 * this buffer will contain the data to be sent FROM THIS device to the connected device
 * this could correspond to sizeof(FEATURES_TEMPLATE), because it's usually the biggest response
 * this device will send. set this according to your needs
 */
#define RESPONSE_MAX_SIZE 1024

/**
 * REQUEST_MAX_SIZE
 *
 * if you don't expect big requests from the remote device, this buffer can be small (usually 128 bytes or less)
 */
#define REQUEST_MAX_SIZE 256

/**
 * WIFI_BUF_MAX_SIZE
 *
 * the longest content is usually the FEATURES array, so the minimum size should be the size of
 * FEATURES_TEMPLATE. usually this is the reason of most hard faults, so try increasing it
 */
#define WIFI_BUF_MAX_SIZE 800

/**
 * UART_BUFFER_SIZE
 *
 * if you have to retrieve large amounts of data (i.e. from an API), set this to the minimum size of the response
 * otherwise, it can be smaller.
 * if you don't have these requirements, you can set it to a minimum of
 * REQUEST_MAX_SIZE + some headroom to avoid receiving only partial messages
 * if you encounter weird behaviors at runtime, try increasing this buffer size
 */
#define UART_BUFFER_SIZE 1536

#define HOSTNAME_MAX_SIZE 32		// ESPDEVICExxx
#define NAME_MAX_SIZE 32			// human-readable name

#define UART_TX_TIMEOUT 500			// ms
#define UART_RX_IDLE_TIMEOUT 3000	// ms

#define RECONNECTION_DELAY_MINS 1	// minutes
#define RECONNECTION_DELAY_MILLIS RECONNECTION_DELAY_MINS * 60000

typedef struct
{
	char* text;
	uint8_t size;
	bool read;
	bool clear_if_read;
} Notification_t;

extern Notification_t notification;	// defined in wifihandler/wifihandler.c

// ==========================================================================================
// 									IRRIGATION / SCHEDULE
// ==========================================================================================
#define VALVES_NUM 4
#define MIN_WATER_FLOW 30			// liters per hour
#define SCHEDULE_TIME_SIZE 11		// hh:mm-hh:mm
#define DEFAULT_SCHEDULE "00:00-00:00"

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
	int8_t 				hour_close;
	int8_t 				minute_close;
	char				text[SCHEDULE_TIME_SIZE+1];
} Schedule_t;

typedef struct valve
{
	uint8_t 			id;
	uint8_t 			isOpen;
	GPIO_TypeDef* 		gpio_port;
	uint16_t 			gpio_pin;
	struct flow* 		flow;
	struct schedule* 	schedule;
	bool 				has_manual_override;
} Valve_t;

// ==========================================================================================
// 											WEATHER
// ==========================================================================================
#define PROBABLE_PRECIPITATION 40			// %, PROB40
#define LOW_PROB_PRECIPITATION 30			// %, PROB30
#define PRECIPITATION_THRESHOLD	1			// millimeters

// ==========================================================================================
// 										SAVE DATA
// ==========================================================================================
/**
 * The save data will be written to the last page of the memory bank
 * 					!!!See FLASH section at the top of the file!!!
 */

/**
* Schedules are saved consecutively without separators, like
* 08:00-08:150830-08:45 (these are two consecutive schedules)
* They can be retrieved by reading SCHEDULE_TIME_SIZE at
* each index: SCHEDULE_TIME_SIZE * i
* where i is the index of the valve (0, 1, 2...)
*/
#ifdef ENABLE_SAVE_TO_FLASH
typedef struct sdata
{
	char schedules[SCHEDULE_TIME_SIZE * VALVES_NUM];
	char name[NAME_MAX_SIZE + 1];
	char ip[15 + 1];
} SaveData_t;

extern SaveData_t savedata;
#endif

// ==========================================================================================
// 										COMM TEMPLATE
// ==========================================================================================

/**
 * template:
 * type1$Name:data;
 * type2$Name,additional_feature$feature_Name$data,additional_feature$feature_name$data...;
 *
 * every type must have a numerical ID (typeX - X being the ID).
 * every type must have a name.
 * a type can have additional features, that must be put on the same line of the main feature,
 * preceded by a comma ",".
 * a semicolon ";" must be put at the end of each feature (line).
 *
 * example:
 * switch1$Switch number one,sensor$Switch status$%d;
 * switch2$Switch number two,sensor$Switch status$%d,sensor$Time$%d;
 * timestamp1$Uptime$%d;
 * sensor1$Battery voltage$%d;
 *
 * FEATURE				SYNTAX												OPTIONAL SYNTAX
 * sensor				sensorX$text$%d text
 * switch				switchX$text,status$%d								switchX$switch_name,status$%d,sensor$sensor_name$%d
 *		status has to be 0 or 1, according to the switch state
 * textinput				textinputX$default_text								textinputX$txt_name,button$btn_name$send<command> (without a space)
 * 		text inside the textinput field will be appended at the end of the command to be sent
 * timepicker			timepicker$%s (time data, should be hh:mm-hh:mm)	timepicker$%s,button$btn_name$send<command>
 * timestamp			timestampX$text$d text
 * external				externalX$id
 * 		external features have an ID, read by the android app, which identifies a feature that will be retrieved from
 * 		a server specified on the android app. the server will return the feature itself that will be displayed
 * 		on the device page on the app.
 *
 * 		external features:
 * 		1 = GRAPH
 *			to mark a SENSOR to be put on the graph, append:
 *			$graph_LineLabel (unit)
 *			LineLabel will be the label of the graph data. this unit will be used for the DAYS time frame.
 *
 *			if you need another unit for MONTHS and YEARS, append:
 *			$graph_LineLabel1 (unit1)_LineLabel2 (unit2)
 *			LineLabel1 and unit1 will be used for DAYS; LineLabel2 and unit2 will be used for MONTHS and YEARS
 *
 *			example: "sensor1$Power$%d W$graph_Average power (W)_Energy (Wh);"
 *
 * 		NOTE: external features will only be updated ONCE, every time the device is loaded in the app. The user can manually refresh the data.
 */

typedef struct bat
{
	uint16_t voltage_mv;
	uint16_t voltage_integer;
	uint16_t voltage_decimal;

} Battery_t;

extern Battery_t bat;

static const char MQTT_DISCOVERY_VALVE[] = 
"{\"name\":\"%s\",\"cmd_t\":\"snse/%s/valve%d/set\",\"stat_t\":\"snse/%s/valve%d/state\",\"pl_on\":\"1\",\"pl_off\":\"0\",\"uniq_id\":\"%s_valve%d\",\"dev\":{\"ids\":[\"%s\"],\"name\":\"%s\"}}";

static const char MQTT_DISCOVERY_SENSOR[] = 
"{\"name\":\"%s\",\"stat_t\":\"snse/%s/%s/state\",\"unit_of_meas\":\"%s\",\"dev_cla\":\"%s\",\"stat_cla\":\"measurement\",\"uniq_id\":\"%s_%s\",\"dev\":{\"ids\":[\"%s\"],\"name\":\"%s\"}}";

static const char MQTT_DISCOVERY_TIME[] = 
"{\"name\":\"%s\",\"cmd_t\":\"snse/%s/valve%d/%s/set\",\"stat_t\":\"snse/%s/valve%d/%s/state\",\"uniq_id\":\"%s_valve%d_%s\",\"dev\":{\"ids\":[\"%s\"],\"name\":\"%s\"}}";

static const char OVERTEMP_TEXT[] = "Temperatura massima superata";

static const char NOTIFICATION_WEATHER_NO_VALVE_OPEN[] = "Le valvole non verranno aperte causa pioggia entro le ultime o prossime 12 ore";
static const char NOTIFICATION_LOW_BATTERY[] = "Batteria scarica!";

#endif /* SETTINGS_H_ */