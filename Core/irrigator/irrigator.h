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

typedef struct
{
	uint8_t id;
	uint8_t isOpen;
	GPIO_TypeDef* gpio_port;
	uint16_t gpio_pin;
} Valve_t;

typedef struct
{

} Flow_t;

Response_t AT_HandleRemoteATCommand(WIFI_t* wifi, Connection_t* conn, char* command_ptr);
void VALVE_Init(Valve_t* valve, uint8_t id,  GPIO_TypeDef* valve_port, uint16_t valve_pin);

#endif /* IRRIGATOR_IRRIGATOR_H_ */
