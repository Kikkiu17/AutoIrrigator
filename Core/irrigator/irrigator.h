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

typedef struct valve
{
	uint8_t id;
	uint8_t isOpen;
	GPIO_TypeDef* gpio_port;
	uint16_t gpio_pin;
	struct valve* next_valve;
} Valve_t;

typedef struct
{

} Flow_t;

Response_t AT_ExecuteRemoteATCommand(WIFI_t* wifi, Connection_t* conn, char* command_ptr);
Response_t WIFI_HandleValveRequest(Connection_t* conn, Valve_t* valve1, char* key_ptr);
void VALVE_Init(Valve_t* valve, uint8_t id,  GPIO_TypeDef* valve_port, uint16_t valve_pin);
void VALVE_Open(Valve_t* valve);
void VALVE_Close(Valve_t* valve);

#endif /* IRRIGATOR_IRRIGATOR_H_ */
