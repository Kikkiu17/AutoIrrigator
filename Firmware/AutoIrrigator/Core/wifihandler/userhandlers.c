#include "userhandlers.h"
#include "mqtt_templates.h"
#include "../irrigator/irrigator.h"
#include <stdlib.h>

extern Valve_t valve_list[VALVES_NUM];
extern Battery_t bat;

int32_t flow1_val = 0;
int32_t flow2_val = 0;
int32_t flow3_val = 0;
int32_t total_flow_val = 0;
int32_t bat_voltage_val = 0;
bool rain_factor_active = false;

static int32_t last_sensor_vals[5] = {-1, -1, -1, -1, -1};

void USER_CalculateSensors(void)
{
	flow1_val = (int32_t)valve_list[0].flow->lt_per_hour;
	flow2_val = (int32_t)valve_list[1].flow->lt_per_hour;
	flow3_val = (int32_t)valve_list[2].flow->lt_per_hour;
	total_flow_val = (int32_t)valve_list[3].flow->lt_per_hour;
	bat_voltage_val = (int32_t)bat.voltage_integer * 100 + (int32_t)bat.voltage_decimal;
}

void USER_PublishSensors(WIFI_t* wifi)
{
	static const char* const ids[5] = {"flow1", "flow2", "flow3", "total_flow", "battery"};
	int32_t* const vals[5] = {&flow1_val, &flow2_val, &flow3_val, &total_flow_val, &bat_voltage_val};

	for (int i = 0; i < 5; i++)
	{
		if (*vals[i] != last_sensor_vals[i])
		{
			WIFIHANDLER_MQTT_PubStateSensorByID(wifi, ids[i]);
			last_sensor_vals[i] = *vals[i];
		}
	}
}

void USER_MQTT_CustomInit(WIFI_t* wifi)
{
	char sub_topic[128];
	// Subscribe to wildcard valve schedule topics: snse/<hostname>/+/+/set
	snprintf(sub_topic, sizeof(sub_topic), "snse/%s/+/+/set", wifi->hostname);
	WIFI_MQTT_Subscribe(wifi, sub_topic, 1);
}

void USER_MQTT_CustomDiscovery(WIFI_t* wifi)
{
	char topic[128];
	char payload[WIFI_BUF_MAX_SIZE];

	const char* valve_names[VALVES_NUM] = {
		"Valvola Ovest",
		"Valvola Sud",
		"Valvola Sud-Est",
		"Valvola Est"
	};

	// Schedules 1-4 (Start Time and Duration)
	for (int i = 0; i < VALVES_NUM; i++)
	{
		char entity_name[48];

		// Start Time
		snprintf(topic, sizeof(topic), "homeassistant/time/%s_valve%d_start/config", wifi->hostname, i + 1);
		snprintf(entity_name, sizeof(entity_name), "%s Inizio", valve_names[i]);
		snprintf(payload, sizeof(payload), MQTT_DISCOVERY_TIME,
				 entity_name, wifi->hostname, i + 1, wifi->hostname, i + 1,
				 wifi->hostname, i + 1, wifi->hostname, wifi->name);
		WIFI_MQTT_Publish(wifi, topic, payload, 1, 1);

		// Duration (in minutes)
		snprintf(topic, sizeof(topic), "homeassistant/number/%s_valve%d_duration/config", wifi->hostname, i + 1);
		snprintf(entity_name, sizeof(entity_name), "%s Durata", valve_names[i]);
		snprintf(payload, sizeof(payload), MQTT_DISCOVERY_DURATION,
				 entity_name, wifi->hostname, i + 1,
				 wifi->hostname, i + 1,
				 wifi->hostname, i + 1,
				 wifi->hostname, wifi->name);
		WIFI_MQTT_Publish(wifi, topic, payload, 1, 1);
	}

	// Rain Factor binary sensor
	snprintf(topic, sizeof(topic), "homeassistant/binary_sensor/%s_rain_factor/config", wifi->hostname);
	snprintf(payload, sizeof(payload),
			 "{\"name\":\"Fattore pioggia\",\"stat_t\":\"snse/%s/rain_factor/state\",\"dev_cla\":\"moisture\",\"uniq_id\":\"%s_rain_factor\",\"dev\":{\"ids\":[\"%s\"],\"name\":\"%s\"}}",
			 wifi->hostname, wifi->hostname, wifi->hostname, wifi->name);
	WIFI_MQTT_Publish(wifi, topic, payload, 1, 1);
}

void USER_MQTT_PublishRainFactor(WIFI_t* wifi, bool is_factor)
{
	rain_factor_active = is_factor;
	char topic[64];
	snprintf(topic, sizeof(topic), "snse/%s/rain_factor/state", wifi->hostname);
	WIFI_MQTT_Publish(wifi, topic, is_factor ? "ON" : "OFF", 0, 1);
}

void USER_MQTT_CustomPublishStates(WIFI_t* wifi)
{
	char topic[128];
	char payload[16];

	// Schedules 1-4 states (Start times and Durations)
	for (int i = 0; i < VALVES_NUM; i++)
	{
		snprintf(topic, sizeof(topic), "snse/%s/valve%d/start/state", wifi->hostname, i + 1);
		snprintf(payload, sizeof(payload), "%02d:%02d", (int)valve_list[i].schedule->hour_open, (int)valve_list[i].schedule->minute_open);
		WIFI_MQTT_Publish(wifi, topic, payload, 0, 1);

		snprintf(topic, sizeof(topic), "snse/%s/valve%d/duration/state", wifi->hostname, i + 1);
		snprintf(payload, sizeof(payload), "%u", (unsigned int)valve_list[i].schedule->duration);
		WIFI_MQTT_Publish(wifi, topic, payload, 0, 1);
	}

	// Rain Factor state
	USER_MQTT_PublishRainFactor(wifi, rain_factor_active);
}

void USER_MQTT_CustomHandler(WIFI_t* wifi, const char* topic, const char* payload)
{
	char expected_topic[128];

	// Check for schedule set topics
	for (int i = 1; i <= VALVES_NUM; i++)
	{
		// Start Time Set
		snprintf(expected_topic, sizeof(expected_topic), "snse/%s/valve%d/start/set", wifi->hostname, i);
		if (strcmp(topic, expected_topic) == 0)
		{
			if (strlen(payload) >= 5)
			{
				int32_t hour = bufferToInt((char*)payload, 2);
				int32_t minute = bufferToInt((char*)payload + 3, 2);
				if (hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59)
				{
					valve_list[i - 1].schedule->hour_open = (int8_t)hour;
					valve_list[i - 1].schedule->minute_open = (int8_t)minute;

					SCHEDULE_UpdateText(valve_list[i - 1].schedule);
					SCHEDULE_Save(valve_list, VALVES_NUM);
					USER_MQTT_CustomPublishStates(wifi);
				}
			}
			return;
		}

		// Duration Set (in minutes)
		snprintf(expected_topic, sizeof(expected_topic), "snse/%s/valve%d/duration/set", wifi->hostname, i);
		if (strcmp(topic, expected_topic) == 0)
		{
			int32_t dur = atoi(payload);
			if (dur >= 0 && dur <= 1440)
			{
				valve_list[i - 1].schedule->duration = (uint16_t)dur;
				SCHEDULE_UpdateText(valve_list[i - 1].schedule);
				SCHEDULE_Save(valve_list, VALVES_NUM);
				USER_MQTT_CustomPublishStates(wifi);
			}
			return;
		}
	}
}
