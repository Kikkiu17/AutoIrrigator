/*
 * valve.h
 *
 *  Created on: Apr 7, 2025
 *      Author: Kikkiu
 */

#ifndef IRRIGATOR_IRRIGATOR_H_
#define IRRIGATOR_IRRIGATOR_H_

#include "../ESP8266/esp8266.h"
#include "../Flash/flash.h"
#include "../settings.h"
#include <stdio.h>
#include <string.h>

void SCHEDULE_Save(Valve_t* valve_list, uint8_t valves_nb);
uint8_t SCHEDULE_SetSchedule(Valve_t* valve, char* new_schedule);
void SCHEDULE_WriteToFlash(Valve_t* valve_list, uint8_t valves_nb);
void SCHEDULE_ReadFromFlash(Valve_t* valve_list, uint8_t valves_nb);
void VALVE_Init(Valve_t* valve, Flow_t* flow, Schedule_t* schedule, uint8_t id,  GPIO_TypeDef* valve_port, uint16_t valve_pin);
void VALVE_Open(Valve_t* valve);
void VALVE_Close(Valve_t* valve);
void FLOW_CalculateFlow(Flow_t* flow);

#endif /* IRRIGATOR_IRRIGATOR_H_ */
