/*
 * wifihandler.c
 *
 * Modified for: MQTT Client
 */

#include "wifihandler.h"
#include <stdio.h>
#include <string.h>

Notification_t notification;

void NOTIFICATION_Set(char* text, uint8_t size)
{
	notification.text = text;
	notification.size = size;
	
	extern WIFI_t wifi;
	if (text != NULL) {
		WIFIHANDLER_MQTT_SendNotification(&wifi, text);
	}
}

void NOTIFICATION_Reset(void)
{
	notification.text = NULL;
	notification.size = 0;
	
	extern WIFI_t wifi;
	WIFIHANDLER_MQTT_SendNotification(&wifi, "");
}

void WIFIHANDLER_MQTT_SendNotification(WIFI_t* wifi, const char* message)
{
    char topic[128];
    snprintf(topic, sizeof(topic), "snse/%s/notify", wifi->hostname);
    WIFI_MQTT_Publish(wifi, topic, message, 1, 0);
}

static uint32_t reconnect_time = 0;

Response_t WIFIHANDLER_ReconnectIfDisconnected(WIFI_t *wifi)
{
    Response_t status = WAITING;
    if (HAL_GetTick() - reconnect_time > RECONNECT_CHECK_INTERVAL)
    {
        reconnect_time = HAL_GetTick();
        
        if (WIFI_MQTT_IsConnected(wifi) == OK)
        {
            return OK;
        }
        
        status = WIFI_Connect(wifi);
        if (status == OK)
        {
            if (WIFI_MQTT_IsConnected(wifi) != OK)
            {
                if (WIFIHANDLER_MQTT_Init(wifi, MQTT_BROKER_IP, MQTT_BROKER_PORT) == OK)
                {
                    WIFIHANDLER_MQTT_PublishDiscovery(wifi);
                    extern Valve_t valve_list[VALVES_NUM];
                    WIFIHANDLER_MQTT_PublishStates(wifi, valve_list);
                }
            }
        }
    }
    return status;
}

Response_t WIFIHANDLER_MQTT_Init(WIFI_t* wifi, const char* broker_ip, uint16_t port)
{
	if (WIFI_MQTT_IsConnected(wifi) != OK)
	{
		if (WIFI_MQTT_Config(wifi, wifi->hostname) != OK) return ERR;
		if (WIFI_MQTT_ConnectBroker(wifi, broker_ip, port) != OK) return ERR;
	}
	
	char sub_topic[128];
	
	// Subscribe to wildcard valve command topics: snse/<hostname>/+/set
	snprintf(sub_topic, sizeof(sub_topic), "snse/%s/+/set", wifi->hostname);
	WIFI_MQTT_Subscribe(wifi, sub_topic, 1);
	
	// Subscribe to wildcard valve schedule topics: snse/<hostname>/+/+/set
	snprintf(sub_topic, sizeof(sub_topic), "snse/%s/+/+/set", wifi->hostname);
	WIFI_MQTT_Subscribe(wifi, sub_topic, 1);

	return OK;
}

void WIFIHANDLER_MQTT_PublishDiscovery(WIFI_t* wifi)
{
	char topic[128];
	char payload[WIFI_BUF_MAX_SIZE];

	// Valve switches
	const char* valve_names[VALVES_NUM] = {
		"West Valve",
		"South Valve",
		"South-East Valve",
		"East Valve"
	};

	for (int i = 0; i < VALVES_NUM; i++)
	{
		snprintf(topic, sizeof(topic), "homeassistant/switch/%s_valve%d/config", wifi->hostname, i + 1);
		snprintf(payload, sizeof(payload), MQTT_DISCOVERY_VALVE,
				 valve_names[i], wifi->hostname, i + 1, wifi->hostname, i + 1,
				 wifi->hostname, i + 1, wifi->hostname, wifi->name);
		WIFI_MQTT_Publish(wifi, topic, payload, 1, 1);
	}

	// Flow sensors 1, 2, 3
	const char* flow_names[3] = {
		"West Flow",
		"South Flow",
		"South-East Flow"
	};
	const char* flow_topics[3] = {
		"flow1",
		"flow2",
		"flow3"
	};

	// Flow sensors 1, 2, 3
    for (int i = 0; i < 3; i++)
    {
        snprintf(topic, sizeof(topic), "homeassistant/sensor/%s_%s/config", wifi->hostname, flow_topics[i]);
        snprintf(payload, sizeof(payload), MQTT_DISCOVERY_SENSOR,
                 flow_names[i], wifi->hostname, flow_topics[i], "L/h",
                 "volume_flow_rate", flow_topics[i], wifi->hostname, wifi->hostname, wifi->name); // Added wifi->hostname
        WIFI_MQTT_Publish(wifi, topic, payload, 1, 1);
    }

    // Total Flow sensor
    snprintf(topic, sizeof(topic), "homeassistant/sensor/%s_total_flow/config", wifi->hostname);
    snprintf(payload, sizeof(payload), MQTT_DISCOVERY_SENSOR,
             "Total Flow", wifi->hostname, "total_flow", "L/h",
             "volume_flow_rate", "total_flow", wifi->hostname, wifi->hostname, wifi->name); // Added wifi->hostname
    WIFI_MQTT_Publish(wifi, topic, payload, 1, 1);

    // Battery sensor
    snprintf(topic, sizeof(topic), "homeassistant/sensor/%s_battery/config", wifi->hostname);
    snprintf(payload, sizeof(payload), MQTT_DISCOVERY_SENSOR,
             "Battery Voltage", wifi->hostname, "battery", "V",
             "voltage", "battery", wifi->hostname, wifi->hostname, wifi->name); // Added wifi->hostname
    WIFI_MQTT_Publish(wifi, topic, payload, 1, 1);

	// Time Pickers (Schedules 1-4)
	for (int i = 0; i < VALVES_NUM; i++)
	{
		char time_name[64];
		
		// Start Time
		snprintf(topic, sizeof(topic), "homeassistant/time/%s_valve%d_start/config", wifi->hostname, i + 1);
		snprintf(time_name, sizeof(time_name), "%s Start Time", valve_names[i]);
		snprintf(payload, sizeof(payload), MQTT_DISCOVERY_TIME,
				 time_name, wifi->hostname, i + 1, "start", wifi->hostname, i + 1, "start",
				 wifi->hostname, i + 1, "start", wifi->hostname, wifi->name);
		WIFI_MQTT_Publish(wifi, topic, payload, 1, 1);
		
		// End Time
		snprintf(topic, sizeof(topic), "homeassistant/time/%s_valve%d_end/config", wifi->hostname, i + 1);
		snprintf(time_name, sizeof(time_name), "%s End Time", valve_names[i]);
		snprintf(payload, sizeof(payload), MQTT_DISCOVERY_TIME,
				 time_name, wifi->hostname, i + 1, "end", wifi->hostname, i + 1, "end",
				 wifi->hostname, i + 1, "end", wifi->hostname, wifi->name);
		WIFI_MQTT_Publish(wifi, topic, payload, 1, 1);
	}
}

void WIFIHANDLER_MQTT_PublishStates(WIFI_t* wifi, Valve_t* valve_list)
{
	char topic[128];
	char payload[64];

	// Valve switch states (valve 1 to 4)
	for (int i = 0; i < VALVES_NUM; i++)
	{
		snprintf(topic, sizeof(topic), "snse/%s/valve%d/state", wifi->hostname, i + 1);
		snprintf(payload, sizeof(payload), "%d", valve_list[i].isOpen);
		WIFI_MQTT_Publish(wifi, topic, payload, 1, 1);
	}

	// Flow sensor states 1, 2, 3
	const char* flow_topics[3] = {
		"flow1",
		"flow2",
		"flow3"
	};
	for (int i = 0; i < 3; i++)
	{
		snprintf(topic, sizeof(topic), "snse/%s/%s/state", wifi->hostname, flow_topics[i]);
		snprintf(payload, sizeof(payload), "%lu", (unsigned long)valve_list[i].flow->lt_per_hour);
		WIFI_MQTT_Publish(wifi, topic, payload, 1, 1);
	}

	// Total Flow sensor state
	snprintf(topic, sizeof(topic), "snse/%s/total_flow/state", wifi->hostname);
	snprintf(payload, sizeof(payload), "%lu", (unsigned long)valve_list[3].flow->lt_per_hour);
	WIFI_MQTT_Publish(wifi, topic, payload, 1, 1);

	// Battery sensor state
	snprintf(topic, sizeof(topic), "snse/%s/battery/state", wifi->hostname);
	snprintf(payload, sizeof(payload), "%d.%02d", bat.voltage_integer, bat.voltage_decimal);
	WIFI_MQTT_Publish(wifi, topic, payload, 1, 1);

	// Schedules 1-4 states (Start and End times)
	for (int i = 0; i < VALVES_NUM; i++)
	{
		snprintf(topic, sizeof(topic), "snse/%s/valve%d/start/state", wifi->hostname, i + 1);
		snprintf(payload, sizeof(payload), "%02d:%02d", valve_list[i].schedule->hour_open, valve_list[i].schedule->minute_open);
		WIFI_MQTT_Publish(wifi, topic, payload, 1, 1);
		
		snprintf(topic, sizeof(topic), "snse/%s/valve%d/end/state", wifi->hostname, i + 1);
		snprintf(payload, sizeof(payload), "%02d:%02d", valve_list[i].schedule->hour_close, valve_list[i].schedule->minute_close);
		WIFI_MQTT_Publish(wifi, topic, payload, 1, 1);
	}
}

void WIFIHANDLER_MQTT_Loop(WIFI_t* wifi, Valve_t* valve_list)
{
	char topic_in[128];
	char payload_in[64];

	if (WIFI_MQTT_Receive(wifi, topic_in, payload_in, 1) == OK)
	{
		char expected_topic[128];
		
		// Check for valve command topics
		for (int i = 1; i <= VALVES_NUM; i++)
		{
			snprintf(expected_topic, sizeof(expected_topic), "snse/%s/valve%d/set", wifi->hostname, i);
			if (strcmp(topic_in, expected_topic) == 0)
			{
				Valve_t* valve = &valve_list[i - 1];
				if (strcmp(payload_in, "1") == 0)
				{
					VALVE_Open(valve);
					valve->has_manual_override = true;
				}
				else if (strcmp(payload_in, "0") == 0)
				{
					VALVE_Close(valve);
					valve->has_manual_override = false;
				}
				
				// Publish state immediately
				char state_topic[128];
				char state_payload[16];
				snprintf(state_topic, sizeof(state_topic), "snse/%s/valve%d/state", wifi->hostname, i);
				snprintf(state_payload, sizeof(state_payload), "%d", valve->isOpen);
				WIFI_MQTT_Publish(wifi, state_topic, state_payload, 1, 1);
				break;
			}
		}

		// Check for schedule set topics
		for (int i = 1; i <= VALVES_NUM; i++)
		{
			// Start Time Set
			snprintf(expected_topic, sizeof(expected_topic), "snse/%s/valve%d/start/set", wifi->hostname, i);
			if (strcmp(topic_in, expected_topic) == 0)
			{
				if (strlen(payload_in) >= 5)
				{
					int32_t hour = bufferToInt(payload_in, 2);
					int32_t minute = bufferToInt(payload_in + 3, 2);
					if (hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59)
					{
						valve_list[i - 1].schedule->hour_open = hour;
						valve_list[i - 1].schedule->minute_open = minute;
						
						// Rebuild text schedule
						snprintf(valve_list[i - 1].schedule->text, sizeof(valve_list[i - 1].schedule->text),
								 "%02d:%02d-%02d:%02d",
								 valve_list[i - 1].schedule->hour_open, valve_list[i - 1].schedule->minute_open,
								 valve_list[i - 1].schedule->hour_close, valve_list[i - 1].schedule->minute_close);
						
						SCHEDULE_Save(valve_list, VALVES_NUM);
						
						// Publish state immediately
						char state_topic[128];
						char state_payload[16];
						snprintf(state_topic, sizeof(state_topic), "snse/%s/valve%d/start/state", wifi->hostname, i);
						snprintf(state_payload, sizeof(state_payload), "%02d:%02d", hour, minute);
						WIFI_MQTT_Publish(wifi, state_topic, state_payload, 1, 1);
					}
				}
				break;
			}
			
			// End Time Set
			snprintf(expected_topic, sizeof(expected_topic), "snse/%s/valve%d/end/set", wifi->hostname, i);
			if (strcmp(topic_in, expected_topic) == 0)
			{
				if (strlen(payload_in) >= 5)
				{
					int32_t hour = bufferToInt(payload_in, 2);
					int32_t minute = bufferToInt(payload_in + 3, 2);
					if (hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59)
					{
						valve_list[i - 1].schedule->hour_close = hour;
						valve_list[i - 1].schedule->minute_close = minute;
						
						// Rebuild text schedule
						snprintf(valve_list[i - 1].schedule->text, sizeof(valve_list[i - 1].schedule->text),
								 "%02d:%02d-%02d:%02d",
								 valve_list[i - 1].schedule->hour_open, valve_list[i - 1].schedule->minute_open,
								 valve_list[i - 1].schedule->hour_close, valve_list[i - 1].schedule->minute_close);
						
						SCHEDULE_Save(valve_list, VALVES_NUM);
						
						// Publish state immediately
						char state_topic[128];
						char state_payload[16];
						snprintf(state_topic, sizeof(state_topic), "snse/%s/valve%d/end/state", wifi->hostname, i);
						snprintf(state_payload, sizeof(state_payload), "%02d:%02d", hour, minute);
						WIFI_MQTT_Publish(wifi, state_topic, state_payload, 1, 1);
					}
				}
				break;
			}
		}
		ESP8266_ClearBuffer();
	}
}