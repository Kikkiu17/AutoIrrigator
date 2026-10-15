/*
 * irrigator.c
 *
 *  Created on: Apr 7, 2025
 *      Author: Kikkiu
 */

#include "irrigator.h"
#include "../wifihandler/wifihandler.h"

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "usart.h"

uint8_t SCHEDULE_SetSchedule(Valve_t* valve, char* new_schedule)
{
	Schedule_t* schedule = valve->schedule;
	// Format: hh:mm-dddd (or legacy hh:mm-hh:mm)
	schedule->hour_open = bufferToInt(new_schedule, 2);
	schedule->minute_open = bufferToInt(new_schedule + 3, 2);

	if (schedule->hour_open < 0 || schedule->hour_open > 23 ||
	    schedule->minute_open < 0 || schedule->minute_open > 59)
		return 0;

	if (new_schedule[5] == '-' && new_schedule[8] == ':')
	{
		// Legacy format hh:mm-hh:mm
		int32_t hour_close = bufferToInt(new_schedule + 6, 2);
		int32_t minute_close = bufferToInt(new_schedule + 9, 2);
		if (hour_close < 0 || hour_close > 23 || minute_close < 0 || minute_close > 59)
			return 0;

		int32_t start_m = (int32_t)schedule->hour_open * 60 + schedule->minute_open;
		int32_t end_m = hour_close * 60 + minute_close;
		if (start_m == end_m)
			schedule->duration = 0;
		else if (end_m > start_m)
			schedule->duration = (uint16_t)(end_m - start_m);
		else
			schedule->duration = (uint16_t)(1440 - start_m + end_m);
	}
	else
	{
		// Format hh:mm-dddd
		char dur_buf[6] = {0};
		strncpy(dur_buf, new_schedule + 6, 5);
		int32_t dur = atoi(dur_buf);
		if (dur < 0 || dur > 1440)
			dur = 0;
		schedule->duration = (uint16_t)dur;
	}

	SCHEDULE_UpdateText(schedule);
	return 1;
}

void SCHEDULE_UpdateText(Schedule_t* schedule)
{
	if (!schedule) return;

	snprintf(schedule->text, sizeof(schedule->text), "%02d:%02d-%04u",
			 (int)schedule->hour_open, (int)schedule->minute_open,
			 (unsigned int)schedule->duration);
}

void SCHEDULE_ReadFromFlash(Valve_t* valve_list, uint8_t valves_nb)
{
	FLASH_ReadSaveData();

	for (uint8_t i = 0; i < valves_nb; i++)
	{
		Schedule_t* schedule = valve_list[i].schedule;
		char* new_schedule = savedata.schedules + SCHEDULE_TIME_SIZE * i;
		if (new_schedule[0] == (char)0xFF)
			memcpy(new_schedule, DEFAULT_SCHEDULE, SCHEDULE_TIME_SIZE);

		schedule->hour_open = bufferToInt(new_schedule, 2);
		schedule->minute_open = bufferToInt(new_schedule + 3, 2);

		if (schedule->hour_open < 0 || schedule->hour_open > 23) schedule->hour_open = 0;
		if (schedule->minute_open < 0 || schedule->minute_open > 59) schedule->minute_open = 0;

		if (new_schedule[5] == '-' && new_schedule[8] == ':')
		{
			// Legacy format hh:mm-hh:mm
			int32_t hour_close = bufferToInt(new_schedule + 6, 2);
			int32_t minute_close = bufferToInt(new_schedule + 9, 2);
			if (hour_close >= 0 && hour_close <= 23 && minute_close >= 0 && minute_close <= 59)
			{
				int32_t start_m = (int32_t)schedule->hour_open * 60 + schedule->minute_open;
				int32_t end_m = hour_close * 60 + minute_close;
				if (start_m == end_m)
					schedule->duration = 0;
				else if (end_m > start_m)
					schedule->duration = (uint16_t)(end_m - start_m);
				else
					schedule->duration = (uint16_t)(1440 - start_m + end_m);
			}
			else
			{
				schedule->duration = 0;
			}
		}
		else
		{
			char dur_buf[6] = {0};
			strncpy(dur_buf, new_schedule + 6, 5);
			int32_t dur = atoi(dur_buf);
			if (dur < 0 || dur > 1440)
				dur = 0;
			schedule->duration = (uint16_t)dur;
		}

		SCHEDULE_UpdateText(schedule);
	}
}

void SCHEDULE_Save(Valve_t* valve_list, uint8_t valves_nb)
{
	for (uint8_t i = 0; i < valves_nb; i++)
	{
		SCHEDULE_UpdateText(valve_list[i].schedule);
		memcpy(savedata.schedules + SCHEDULE_TIME_SIZE * i, valve_list[i].schedule->text, SCHEDULE_TIME_SIZE);
	}
	FLASH_WriteSaveData();
}

void VALVE_Init(Valve_t* valve, Flow_t* flow, Schedule_t* schedule, uint8_t id, GPIO_TypeDef* valve_port, uint16_t valve_pin, Switch_t* sw)
{
	valve->flow = flow;
	valve->schedule = schedule;
	valve->id = id;
	valve->gpio_port = valve_port;
	valve->gpio_pin = valve_pin;
	valve->has_manual_override = false;
	valve->isOpen = 0;
	valve->sw = sw;
}

void VALVE_Open(Valve_t* valve)
{
	valve->isOpen = 1;
	if (valve->sw != NULL)
		SWITCH_Press(valve->sw);
	else
		HAL_GPIO_WritePin(valve->gpio_port, valve->gpio_pin, GPIO_PIN_SET);
}

void VALVE_Close(Valve_t* valve)
{
	valve->isOpen = 0;
	if (valve->sw != NULL)
		SWITCH_UnPress(valve->sw);
	else
		HAL_GPIO_WritePin(valve->gpio_port, valve->gpio_pin, GPIO_PIN_RESET);
}

void FLOW_CalculateFlow(Flow_t* flow)
{
	if (flow->ic_val > flow->last_ic_val)
	{
		flow->ic_timestamp = uwTick;
		flow->period_us = (flow->ic_val - flow->last_ic_val) * 8; // PSC is 255
		flow->lt_per_hour = 8000000 / flow->period_us;
	}
	flow->last_ic_val = flow->ic_val;
	if (flow->lt_per_hour < MIN_WATER_FLOW)
		flow->lt_per_hour = 0;
}
