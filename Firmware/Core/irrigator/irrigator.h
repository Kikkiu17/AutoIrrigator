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

typedef struct flow
{
	uint32_t last_ic_val;
	uint32_t ic_val;
	uint32_t ic_timestamp;
	uint32_t period_us;
	uint32_t lt_per_hour;
} Flow_t;

typedef struct valve
{
	uint8_t id;
	char status[6];
	GPIO_TypeDef* gpio_port;
	uint16_t gpio_pin;
	struct flow* flow;
} Valve_t;

Response_t AT_ExecuteRemoteATCommand(Connection_t* conn, char* command_ptr);
Response_t WIFIHANDLER_HandleValveRequest(Connection_t* conn, Valve_t* valve_list, uint32_t list_size, char* key_ptr);

void VALVE_Init(Valve_t* valve, Flow_t* flow, uint8_t id,  GPIO_TypeDef* valve_port, uint16_t valve_pin);
void VALVE_Open(Valve_t* valve);
void VALVE_Close(Valve_t* valve);
void FLOW_CalculateFlow(Flow_t* flow);

#endif /* IRRIGATOR_IRRIGATOR_H_ */
