/*
 * valve.c
 *
 *  Created on: Apr 7, 2025
 *      Author: Kikkiu
 */

#include "irrigator.h"

#include <stdio.h>
#include <inttypes.h>
#include "usart.h"

uint8_t SCHEDULE_SetSchedule(Valve_t* valve, char* new_schedule)
{
	Schedule_t* schedule = valve->schedule;
	// hh:mm-hh:mm
	schedule->hour_open = bufferToInt(new_schedule, 2);
	schedule->minute_open = bufferToInt(new_schedule + 3, 2);
	schedule->hour_close = bufferToInt(new_schedule + 6, 2);
	schedule->minute_close = bufferToInt(new_schedule + 9, 2);

	if (schedule->hour_open == -1 || schedule->minute_open == -1 || schedule->hour_close == -1 || schedule->minute_close == -1)
		return 0;

	memcpy(schedule->text, new_schedule, SCHEDULE_TIME_SIZE);
	schedule->text[SCHEDULE_TIME_SIZE] = '\0';
	return 1;
}

void SCHEDULE_ReadFromFlash(Valve_t* valve_list, uint8_t valves_nb)
{
	FLASH_ReadSaveData();

	for (uint8_t i = 0; i < valves_nb; i++)
	{
		Schedule_t* schedule = valve_list[i].schedule;
		char* new_schedule = savedata.schedules + SCHEDULE_TIME_SIZE * i;
		if (new_schedule[0] == 0xFF)
			memcpy(new_schedule, DEFAULT_SCHEDULE, SCHEDULE_TIME_SIZE);
		// hh:mm-hh:mm
		schedule->hour_open = bufferToInt(new_schedule, 2);
		schedule->minute_open = bufferToInt(new_schedule + 3, 2);
		schedule->hour_close = bufferToInt(new_schedule + 6, 2);
		schedule->minute_close = bufferToInt(new_schedule + 9, 2);
		memcpy(schedule->text, new_schedule, SCHEDULE_TIME_SIZE);
	}
}

void SCHEDULE_Save(Valve_t* valve_list, uint8_t valves_nb)
{
	for (uint8_t i = 0; i < valves_nb; i++)
	{
		memcpy(savedata.schedules + SCHEDULE_TIME_SIZE * i, valve_list[i].schedule->text, SCHEDULE_TIME_SIZE);
	}
	FLASH_WriteSaveData();
}



void VALVE_Init(Valve_t* valve, Flow_t* flow, Schedule_t* schedule, uint8_t id, GPIO_TypeDef* valve_port, uint16_t valve_pin)
{
	valve->flow = flow;
	valve->schedule = schedule;
	valve->id = id;
	valve->gpio_port = valve_port;
	valve->gpio_pin = valve_pin;
	valve->has_manual_override = false;
}

void VALVE_Open(Valve_t* valve)
{
	valve->isOpen = 1;
	HAL_GPIO_WritePin(valve->gpio_port, valve->gpio_pin, 1);
}

void VALVE_Close(Valve_t* valve)
{
	valve->isOpen = 0;
	HAL_GPIO_WritePin(valve->gpio_port, valve->gpio_pin, 0);
}

void FLOW_CalculateFlow(Flow_t* flow)
{
	if (flow->ic_val > flow->last_ic_val)
	{
		flow->ic_timestamp = uwTick;
		flow->period_us = (flow->ic_val - flow->last_ic_val) * 8; // PSC is 255
		//flow->lt_per_hour = 1.0 / ((float)flow->period_us / 1000000.0) / 7.5 * 60.0;
		flow->lt_per_hour = 8000000 / flow->period_us;
	}
	flow->last_ic_val = flow->ic_val;
	if (flow->lt_per_hour < MIN_WATER_FLOW)
		flow->lt_per_hour = 0;
}
