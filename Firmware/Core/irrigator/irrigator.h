/*
 * valve.h
 *
 *  Created on: Apr 7, 2025
 *      Author: Kikkiu
 */

#ifndef IRRIGATOR_IRRIGATOR_H_
#define IRRIGATOR_IRRIGATOR_H_

#include "../ESP8266/esp8266.h"
#include <stdio.h>
#include <string.h>

#define MIN_WATER_FLOW 30			// liters per hour
#define SCHEDULE_SIZE 11			// hh:mm-hh:mm

typedef struct flow
{
	uint32_t last_ic_val;
	uint32_t ic_val;
	uint32_t ic_timestamp;
	uint32_t period_us;
	uint32_t lt_per_hour;
} Flow_t;

typedef struct schedule
{
	int8_t hour_open;
	int8_t minute_open;
	int8_t hour_close;
	int8_t minute_close;
	char	text[11];
} Schedule_t;

typedef struct valve
{
	uint8_t id;
	uint8_t isOpen;
	char status[6];
	GPIO_TypeDef* gpio_port;
	uint16_t gpio_pin;
	struct flow* flow;
	struct schedule* schedule;
} Valve_t;

Response_t AT_ExecuteRemoteATCommand(Connection_t* conn, char* command_ptr);
Response_t WIFIHANDLER_HandleValveRequest(Connection_t* conn, Valve_t* valve_list, uint32_t list_size, char* key_ptr);

void VALVE_Init(Valve_t* valve, Flow_t* flow, Schedule_t* schedule, uint8_t id,  GPIO_TypeDef* valve_port, uint16_t valve_pin);
void VALVE_Open(Valve_t* valve);
void VALVE_Close(Valve_t* valve);
void FLOW_CalculateFlow(Flow_t* flow);
Response_t WIFIHANDLER_HandleScheduleRequest(Connection_t* conn, char* command_ptr, Valve_t* valve);

#endif /* IRRIGATOR_IRRIGATOR_H_ */
