/*
 * features.h
 *
 *  Created on: Aug 6, 2025
 *      Author: kikkiu
 */

#ifndef SETTINGS_H_
#define SETTINGS_H_

typedef uint8_t bool;
#define true 1
#define false 0

// ==========================================================================================
// 											FLASH
// ==========================================================================================
/**
 *				!!!THE LAST PAGE OF THE FLASH MEMORY HAS TO BE BLANK!!!
 * 						!!!CHECK PROGRAM SIZE BEFORE UPLOADING!!!
 */
typedef uint64_t FLASH_DATATYPE;
#define FLASH_DATASIZE sizeof(FLASH_DATATYPE)
#define LAST_PAGE_ADDRESS 0x8007800
//#define LAST_PAGE_ADDRESS 0x08000000 + ((FLASH_PAGE_NB - 1) * FLASH_PAGE_SIZE)
//#define LAST_PAGE_ADDRESS 0x08000000 + FLASH_BANK_SIZE - FLASH_PAGE_SIZE + 1

// ==========================================================================================
// 									NETWORK (esp8266.h)
// ==========================================================================================
static const char ESP_NAME[] = "Hub irrigazione";
#define SERVER_PORT 34677

// NOT SUPPORTED:
//static const char ESP_HOSTNAME[] = "ESPDEVICE002"; // template: ESPDEVICExxx
//static const char ESP_IP[] = "192.168.1.38";

#define AT_SHORT_TIMEOUT 250
#define AT_MEDIUM_TIMEOUT 500

#define RESPONSE_MAX_SIZE 1024
#define REQUEST_MAX_SIZE 256
#define WIFI_BUF_MAX_SIZE 800
#define HOSTNAME_MAX_SIZE 32		// ESPDEVICExxx
#define NAME_MAX_SIZE 32			// human-readable name

#define UART_BUFFER_SIZE 2048
#define UART_TX_TIMEOUT 500	// ms
#define UART_RX_IDLE_TIMEOUT 3000	// ms
#define STM_UART huart1

#define ESP_RST_PORT ESPRST_GPIO_Port
#define ESP_RST_PIN ESPRST_Pin

typedef struct notif
{
	char* text;
	uint8_t size;
} Notification_t;

extern Notification_t notification;

// ==========================================================================================
// 									IRRIGATION / SCHEDULE
// ==========================================================================================
#define VALVES_NUM 4
#define MIN_WATER_FLOW 30			// liters per hour
#define SCHEDULE_TIME_SIZE 11		// hh:mm-hh:mm

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
	char				text[SCHEDULE_TIME_SIZE];
} Schedule_t;

typedef struct valve
{
	uint8_t 			id;
	uint8_t 			isOpen;
	char 				status[6];
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
// 										HELP MESSAGES
// ==========================================================================================
static const char GET_HELP_MESSAGE[] =
{
		"Comandi GET:"
		"\n- valve"
		"\n- wifi"
		"\n- features"
		"\n- weather"
		"\n- help"
		"\n\nper ottenere aiuto per ogni comando, fai <comando>=help"
};

static const char POST_HELP_MESSAGE[] =
{
		"Comandi POST:"
		"\n- at"
		"\n\nper ottenere aiuto per ogni comando, fai <comando>=help"
};

static const char WIFI_HELP_MESSAGE[] =
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

static const char WEATHER_HELP_MESSAGE[] =
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

// ==========================================================================================
// 										SAVE DATA
// ==========================================================================================
/**
 * The save data will be written to the last page of the memory bank
 * See FLASH section at the top of the file
 */

/**
 * Schedules are saved consecutively without separators, like
 * 08:00-08:150830-08:45 (these are two consecutive schedules)
 * They can be retrieved by reading SCHEDULE_TIME_SIZE at
 * each index: SCHEDULE_TIME_SIZE * i
 * where i is the index of the valve (0, 1, 2...)
 */
typedef struct sdata
{
	char schedules[SCHEDULE_TIME_SIZE * VALVES_NUM];
	char name[NAME_MAX_SIZE];
} SaveData_t;

extern SaveData_t savedata;

// ==========================================================================================
// 											OTHER
// ==========================================================================================

// all times are in milliseconds
#define STROBE_DELAY 4000
#define STROBE_DURATION 12

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
 * textinut				textinputX$default_text								textinputX$txt_name,button$btn_name$send<command> (without a space)
 * 		text inside the textinput field will be appended at the end of the command to be sent
 * timepicker			timepicker$%s (time data, should be hh:mm-hh:mm)	timepicker$%s,button$btn_name$send<command>
 * timestamp			timestampX$text$d text
 */

typedef struct bat
{
	uint16_t voltage_mv;
	uint16_t voltage_integer;
	uint16_t voltage_decimal;

} Battery_t;

extern Battery_t bat;

static const char FEATURES_TEMPLATE[] =
{
		"sensor1$Flusso totale$%d litri/h;"
		"sensor2$Tensione batteria$%d,%d V;"
		"switch1$Ovest,status$%d,sensor$Litri/h$%d;"
		"switch2$Sud,status$%d,sensor$Litri/h$%d;"
		"switch3$Sud-Est,status$%d,sensor$Litri/h$%d;"
		"switch4$Est,status$%d;"
		//"textinput1$%s,button$Imposta$textInputPOST ?valve=1&schedule=;"
		//"textinput2$%s,button$Imposta$textInputPOST ?valve=2&schedule=;"
		//"textinput3$%s,button$Imposta$textInputPOST ?valve=3&schedule=;"
		//"textinput4$%s,button$Imposta$textInputPOST ?valve=4&schedule=;"
		"timepicker1$%s,button$Imposta$sendPOST ?valve=1&schedule=;"
		"timepicker2$%s,button$Imposta$sendPOST ?valve=2&schedule=;"
		"timepicker3$%s,button$Imposta$sendPOST ?valve=3&schedule=;"
		"timepicker4$%s,button$Imposta$sendPOST ?valve=4&schedule=;"
		//"textinput1$NOME 1,button$Imposta$sendPOST ?valve=1&cmd=;"
		"timestamp1$Tempo CPU$%d ms;"
};

static const char NOTIFICATION_WEATHER_NO_VALVE_OPEN[] =
{
		"1$Le valvole non verranno aperte causa pioggia entro le ultime o prossime 12 ore"
};

static const char NOTIFICATION_LOW_BATTERY[] =
{
		"2$Batteria scarica!"
};

#endif /* SETTINGS_H_ */
