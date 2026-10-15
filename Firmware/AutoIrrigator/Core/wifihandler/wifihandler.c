/*
 * wifihandler.c
 *
 *  Created on: Apr 12, 2025
 *      Author: Kikkiu
 */

#include "wifihandler.h"
#include "../Flash/flash.h"
#include "mqtt_templates.h"
#include <stdlib.h>

__attribute__((weak)) void USER_MQTT_CustomInit(WIFI_t* wifi) {}
__attribute__((weak)) void USER_MQTT_CustomDiscovery(WIFI_t* wifi) {}
__attribute__((weak)) void USER_MQTT_CustomPublishStates(WIFI_t* wifi) {}
__attribute__((weak)) void USER_MQTT_CustomHandler(WIFI_t* wifi, const char* topic, const char* payload) {}

Switch_t switches[MAX_SWITCHES];
static uint32_t switch_count = 0;

static Sensor_t sensors[MAX_SENSORS];
static uint32_t sensor_count = 0;

#ifdef ENABLE_COVER
static Cover_t covers[MAX_COVERS];
static uint32_t cover_count = 0;
#endif

#ifdef ENABLE_LIGHT
extern Light_t lights[MAX_LIGHTS];
extern uint32_t light_count;
#endif

#ifdef ENABLE_SERVO
Servo_t servos[MAX_SERVOS];
uint32_t servo_count = 0;
#endif

Notification_t notification =
{
	.text = NULL,
	.size = 0,
	.read = 0,
	.clear_if_read = true
};

void SWITCH_Init(Switch_t* sw, const char* switch_id, const char* name, bool inverted, GPIO_TypeDef* port, uint16_t pin)
{
	sw->switch_id = switch_id;
	sw->name = name;
	sw->pressed = false;
	sw->inverted = inverted;
	sw->port = port;
	sw->pin = pin;
	sw->manual = false;
	HAL_GPIO_WritePin(port, pin, inverted ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

Switch_t* SWITCH_GetByID(const char* switch_id)
{
	if (!switch_id) return NULL;
	for (uint32_t i = 0; i < switch_count; i++)
	{
		if (switches[i].switch_id != NULL && strcmp(switches[i].switch_id, switch_id) == 0)
			return &switches[i];
	}
	return NULL;
}

void SWITCH_Press(Switch_t* sw)
{
	sw->pressed = true;
	HAL_GPIO_WritePin(sw->port, sw->pin, sw->inverted ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

void SWITCH_UnPress(Switch_t* sw)
{
	sw->pressed = false;
	HAL_GPIO_WritePin(sw->port, sw->pin, sw->inverted ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void NOTIFICATION_Set(char* text, uint8_t size)
{
	notification.text = text;
	notification.size = size;
	notification.read = false;

	extern WIFI_t wifi;
	if (text != NULL) {
		WIFIHANDLER_MQTT_SendNotification(&wifi, text);
	}
}

void NOTIFICATION_Reset(void)
{
	notification.text = NULL;
	notification.size = 0;
	notification.read = true;

	extern WIFI_t wifi;
	WIFIHANDLER_MQTT_SendNotification(&wifi, "");
}

// reconnect to WiFi and MQTT Broker
static uint32_t reconnect_time = 0;
static bool mqtt_initialized = false;

Response_t WIFIHANDLER_ReconnectIfDisconnected(WIFI_t *wifi)
{
	Response_t status = WAITING;
	if (HAL_GetTick() - reconnect_time > RECONNECT_CHECK_INTERVAL)
	{
		reconnect_time = HAL_GetTick();

		if (mqtt_initialized && WIFI_MQTT_IsConnected(wifi) == OK)
		{
			return OK;
		}

		mqtt_initialized = false;
		status = WIFI_Connect(wifi);
		if (status == OK)
		{
			if (WIFIHANDLER_MQTT_Init(wifi, MQTT_BROKER_IP, MQTT_BROKER_PORT) == OK)
			{
				WIFIHANDLER_MQTT_PublishAllDiscovery(wifi);
				WIFIHANDLER_MQTT_PublishAllStates(wifi);
			}
		}
	}
	return status;
}

// ==========================================================================================
// 										MQTT CLIENT MODE
// ==========================================================================================

Response_t WIFIHANDLER_MQTT_Init(WIFI_t* wifi, const char* broker_ip, uint16_t port)
{
    mqtt_initialized = false;

    if (WIFI_MQTT_IsConnected(wifi) != OK)
    {
        WIFI_MQTT_Clean(wifi);
        if (WIFI_MQTT_Config(wifi, wifi->hostname) != OK) return ERR;
        if (WIFI_MQTT_ConnectBroker(wifi, broker_ip, port) != OK) return ERR;
    }
    
    char sub_topic[128];

    if (WIFI_MQTT_Subscribe(wifi, "homeassistant/status", 1) != OK) return ERR;
    
    // ALL basic ON/OFF commands (covers switches, covers, and lights)
    snprintf(sub_topic, sizeof(sub_topic), "snse/%s/+/set", wifi->hostname);
    if (WIFI_MQTT_Subscribe(wifi, sub_topic, 1) != OK) return ERR;

#ifdef ENABLE_COVER
    // cover position commands
    snprintf(sub_topic, sizeof(sub_topic), "snse/%s/+/pos_set", wifi->hostname);
    if (WIFI_MQTT_Subscribe(wifi, sub_topic, 1) != OK) return ERR;
#endif

#ifdef ENABLE_LIGHT
    // light brightness commands
    snprintf(sub_topic, sizeof(sub_topic), "snse/%s/+/bri_set", wifi->hostname);
    if (WIFI_MQTT_Subscribe(wifi, sub_topic, 1) != OK) return ERR;
#endif

#ifdef ENABLE_SERVO
    // servo min/max angle commands
    snprintf(sub_topic, sizeof(sub_topic), "snse/%s/+/min_set", wifi->hostname);
    if (WIFI_MQTT_Subscribe(wifi, sub_topic, 1) != OK) return ERR;

    snprintf(sub_topic, sizeof(sub_topic), "snse/%s/+/max_set", wifi->hostname);
    if (WIFI_MQTT_Subscribe(wifi, sub_topic, 1) != OK) return ERR;
#endif

    USER_MQTT_CustomInit(wifi);

    mqtt_initialized = true;
    return OK;
}

// ====================================================================
// SENSOR FUNCTIONS
// ====================================================================

bool WIFIHANDLER_MQTT_RegisterSensor(const char* friendly_name, const char* unit,
	const char* sensor_id, const char* sensor_type, int32_t* value_ptr, uint8_t decimals)
{
    if (sensor_count >= MAX_SENSORS || !value_ptr)
        return false;

    sensors[sensor_count].name = friendly_name;
    sensors[sensor_count].unit          = unit;
    sensors[sensor_count].sensor_id     = sensor_id;
    sensors[sensor_count].sensor_type   = sensor_type;
    sensors[sensor_count].value_ptr     = value_ptr;
    sensors[sensor_count].decimals      = decimals;
    
    sensor_count++;
    return true;
}

void WIFIHANDLER_MQTT_PubDiscoverySensor(WIFI_t* wifi, Sensor_t* sensor)
{
	if (!wifi || !sensor) return;

	char topic[128];
	char payload[WIFI_BUF_MAX_SIZE];
	char stat_t[64], uniq_id[64];

	snprintf(topic, sizeof(topic), MQTT_DISCOVERY_SENSOR_TOPIC_TEMPLATE, wifi->hostname, sensor->sensor_id);
	snprintf(stat_t, sizeof(stat_t), MQTT_SENSOR_STATE_TEMPLATE, wifi->hostname, sensor->sensor_id);
	snprintf(uniq_id, sizeof(uniq_id), MQTT_SENSOR_UNIQUE_ID_TEMPLATE, wifi->hostname, sensor->sensor_id);

	snprintf(payload, sizeof(payload), MQTT_DISCOVERY_SENSOR,
		sensor->name, stat_t, sensor->unit, sensor->sensor_type, uniq_id, wifi->hostname, wifi->name);
	WIFI_MQTT_Publish(wifi, topic, payload, 1, 1);
}

void WIFIHANDLER_MQTT_PubStateSensor(WIFI_t* wifi, Sensor_t* sensor)
{
    char topic[128];
    char payload[64];

    snprintf(topic, sizeof(topic), MQTT_STATE_SENSOR_TOPIC_TEMPLATE, wifi->hostname, sensor->sensor_id);

    if (sensor->decimals == 0)
        snprintf(payload, sizeof(payload), "%" PRIi32, *(sensor->value_ptr));
    else
    {
        int32_t raw = *sensor->value_ptr;
        uint8_t dec = (sensor->decimals > 6) ? 6 : sensor->decimals;

        int32_t scale = 1;
        for (uint8_t i = 0; i < dec; i++) scale *= 10;

        int32_t integer_part = raw / scale;
        int32_t frac_part = raw % scale;
        if (frac_part < 0) frac_part = -frac_part;

        snprintf(payload, sizeof(payload), "%" PRIi32 ".%0*" PRIi32, 
                 integer_part, (int16_t)dec, frac_part);
    }

    WIFI_MQTT_Publish(wifi, topic, payload, 0, 1);
}

void WIFIHANDLER_MQTT_PubStateSensorByID(WIFI_t* wifi, const char* sensor_id)
{
    if (!wifi || !sensor_id) return;

    for (uint32_t i = 0; i < sensor_count; i++)
    {
        if (sensors[i].sensor_id != NULL && strcmp(sensors[i].sensor_id, sensor_id) == 0)
        {
            if (sensors[i].value_ptr != NULL)
                WIFIHANDLER_MQTT_PubStateSensor(wifi, &sensors[i]);
            return;
        }
    }
}

// ====================================================================
// SWITCH FUNCTIONS
// ====================================================================

bool WIFIHANDLER_MQTT_RegisterSwitch(const char* switch_id, const char* name, bool inverted, GPIO_TypeDef* port, uint16_t pin, bool default_on)
{
	if (switch_count >= MAX_SWITCHES || !port || !switch_id)
		return false;

	switches[switch_count].switch_id = switch_id;
	switches[switch_count].name = name;
	switches[switch_count].pressed = default_on;
	switches[switch_count].inverted = inverted;
	switches[switch_count].port = port;
	switches[switch_count].pin = pin;
	switches[switch_count].manual = false;
	HAL_GPIO_WritePin(port, pin, default_on ? GPIO_PIN_SET : GPIO_PIN_RESET);
	
	switch_count++;
	return true;
}

void WIFIHANDLER_MQTT_PubDiscoverySwitch(WIFI_t* wifi, Switch_t* sw)
{
	if (!wifi || !sw || !sw->switch_id) return;
	char topic[128];
	char payload[WIFI_BUF_MAX_SIZE];
	char cmd_t[64], stat_t[64], uniq_id[64];

	snprintf(topic, sizeof(topic), MQTT_DISCOVERY_SWITCH_TOPIC_TEMPLATE, wifi->hostname, sw->switch_id);
	snprintf(cmd_t, sizeof(cmd_t), MQTT_SWITCH_CMD_TEMPLATE, wifi->hostname, sw->switch_id);
	snprintf(stat_t, sizeof(stat_t), MQTT_SWITCH_STATE_TEMPLATE, wifi->hostname, sw->switch_id);
	snprintf(uniq_id, sizeof(uniq_id), MQTT_SWITCH_UNIQUE_ID_TEMPLATE, wifi->hostname, sw->switch_id);

	snprintf(payload, sizeof(payload), MQTT_DISCOVERY_SWITCH,
		sw->name, cmd_t, stat_t, uniq_id, wifi->hostname, wifi->name);
	WIFI_MQTT_Publish(wifi, topic, payload, 1, 1);	
}

void WIFIHANDLER_MQTT_PubStateSwitch(WIFI_t* wifi, Switch_t* sw)
{
	if (!wifi || !sw || !sw->switch_id) return;
	char topic[128];
	char payload[64];

	snprintf(topic, sizeof(topic), MQTT_STATE_SWITCH_TOPIC_TEMPLATE, wifi->hostname, sw->switch_id);
	snprintf(payload, sizeof(payload), "%d", sw->pressed);
	WIFI_MQTT_Publish(wifi, topic, payload, 0, 1);
}

void WIFIHANDLER_MQTT_PubStateSwitchByID(WIFI_t* wifi, const char* switch_id)
{
	Switch_t* sw = SWITCH_GetByID(switch_id);
	if (sw != NULL)
	{
		WIFIHANDLER_MQTT_PubStateSwitch(wifi, sw);
	}
}

// ====================================================================
// COVER FUNCTIONS
// ====================================================================
#ifdef ENABLE_COVER
bool WIFIHANDLER_MQTT_RegisterCover(const char* cover_id, const char* friendly_name, STEPPER_DRIVER_HANDLE* stepper)
{
    if (cover_count >= MAX_COVERS || !cover_id || !stepper)
        return false;

    covers[cover_count].cover_id      = cover_id;
    covers[cover_count].friendly_name = friendly_name;
    covers[cover_count].stepper       = stepper;
    covers[cover_count].last_state    = STEPPER_STOPPED;

    cover_count++;
    return true;
}

Cover_t* COVER_GetByID(const char* cover_id)
{
    if (!cover_id) return NULL;
    for (uint32_t i = 0; i < cover_count; i++)
    {
        if (covers[i].cover_id != NULL && strcmp(covers[i].cover_id, cover_id) == 0)
            return &covers[i];
    }
    return NULL;
}

void COVER_Open(Cover_t* cover)
{
    if (!cover || !cover->stepper) return;
    DRV8825_Stop(cover->stepper);
    DRV8825_Wakeup(cover->stepper);
    DRV8825_MoveTo(cover->stepper, cover->stepper->max_position);
}

void COVER_Close(Cover_t* cover)
{
    if (!cover || !cover->stepper) return;
    DRV8825_Stop(cover->stepper);
    DRV8825_Wakeup(cover->stepper);
    DRV8825_MoveTo(cover->stepper, cover->stepper->min_position);
}

void COVER_Stop(Cover_t* cover)
{
    if (!cover || !cover->stepper) return;
    DRV8825_Stop(cover->stepper);
}

void COVER_SetPosition(Cover_t* cover, uint8_t percent)
{
    if (!cover || !cover->stepper) return;
    if (percent > 100) percent = 100;

    int32_t min_p = cover->stepper->min_position;
    int32_t max_p = cover->stepper->max_position;
    int64_t range = (int64_t)max_p - min_p;

    // standard: 0% = min_position (closed), 100% = max_position (open)
    int32_t target_pos = min_p + (int32_t)((range * percent) / 100);

    DRV8825_Stop(cover->stepper);
    DRV8825_Wakeup(cover->stepper);
    DRV8825_MoveTo(cover->stepper, target_pos);
}

void WIFIHANDLER_MQTT_PubDiscoveryCover(WIFI_t* wifi, Cover_t* cover)
{
    if (!wifi || !cover || !cover->cover_id) return;

    char topic[128];
    char payload[WIFI_BUF_MAX_SIZE];

    snprintf(topic, sizeof(topic), "homeassistant/cover/%s_%s/config", wifi->hostname, cover->cover_id);
    
    snprintf(payload, sizeof(payload), MQTT_DISCOVERY_COVER,
            cover->friendly_name, 
            wifi->hostname, cover->cover_id, // cmd_t
            wifi->hostname, cover->cover_id, // stat_t
            wifi->hostname, cover->cover_id, // pos_t
            wifi->hostname, cover->cover_id, // set_pos_t
            wifi->hostname, cover->cover_id, // uniq_id
            wifi->hostname, wifi->name);     // dev

    WIFI_MQTT_Publish(wifi, topic, payload, 1, 1);
}

void WIFIHANDLER_MQTT_PubStateCover(WIFI_t* wifi, Cover_t* cover)
{
    if (!wifi || !cover || !cover->cover_id || !cover->stepper) return;

    char topic[128];
    char payload[64];

    // position percentage
    int32_t min_p = cover->stepper->min_position;
    int32_t max_p = cover->stepper->max_position;
    int64_t range = (int64_t)max_p - min_p;
    uint8_t pos_percent = 0;

    if (range > 0)
    {
        int64_t current_offset = (int64_t)cover->stepper->current_pos - min_p;
        if (current_offset < 0) current_offset = 0;
        if (current_offset > range) current_offset = range;
        pos_percent = (uint8_t)((current_offset * 100) / range);
    }

    // movement state (opening, closing, open, closed)
    snprintf(topic, sizeof(topic), MQTT_STATE_TOPIC_TEMPLATE, wifi->hostname, cover->cover_id);
    if (cover->stepper->state != STEPPER_STOPPED)
    {
        if (cover->stepper->dir_multiplier == 1)
            snprintf(payload, sizeof(payload), "opening");
        else
            snprintf(payload, sizeof(payload), "closing");
    }
    else
    {
        if (pos_percent == 0)
            snprintf(payload, sizeof(payload), "closed");
        else
            snprintf(payload, sizeof(payload), "open");
    }
    WIFI_MQTT_Publish(wifi, topic, payload, 0, 1);

    snprintf(topic, sizeof(topic), MQTT_POSITION_TOPIC_TEMPLATE, wifi->hostname, cover->cover_id);
    snprintf(payload, sizeof(payload), "%u", pos_percent);
    WIFI_MQTT_Publish(wifi, topic, payload, 0, 1);
}

void WIFIHANDLER_MQTT_PubStateCoverByID(WIFI_t* wifi, const char* cover_id)
{
    Cover_t* cover = COVER_GetByID(cover_id);
    if (cover != NULL)
        WIFIHANDLER_MQTT_PubStateCover(wifi, cover);
}
#endif

// ====================================================================
// LIGHT FUNCTIONS
// ====================================================================
#ifdef ENABLE_LIGHT
bool WIFIHANDLER_MQTT_RegisterLight(const char* light_id, const char* friendly_name, 
    TIM_HandleTypeDef* htim, uint32_t channel, const uint16_t* gamma_table, bool inverted)
{
    if (light_count >= MAX_LIGHTS || !light_id || !htim)
        return false;

    lights[light_count].light_id           = light_id;
    lights[light_count].friendly_name      = friendly_name;
    lights[light_count].htim               = htim;
    lights[light_count].channel            = channel;
    lights[light_count].gamma_table        = gamma_table;
    lights[light_count].inverted           = inverted;
    lights[light_count].state              = false;
    lights[light_count].current_brightness = 0;
    lights[light_count].target_brightness  = 0;
    lights[light_count].last_step_time     = 0;
    lights[light_count].fade_step_delay_ms = 4; // ~1 sec full ramp (255 steps * 4ms)

    LIGHT_UpdatePWM(&lights[light_count]);

    light_count++;
    return true;
}

Light_t* LIGHT_GetByID(const char* light_id)
{
    if (!light_id) return NULL;
    for (uint32_t i = 0; i < light_count; i++)
    {
        if (lights[i].light_id != NULL && strcmp(lights[i].light_id, light_id) == 0)
            return &lights[i];
    }
    return NULL;
}

void LIGHT_UpdatePWM(Light_t* light)
{
    if (!light || !light->htim) return;

    uint32_t max_arr = __HAL_TIM_GET_AUTORELOAD(light->htim);
    uint32_t compare_val = 0;

    if (light->current_brightness > 0)
    {
        if (light->gamma_table != NULL)
            compare_val = light->gamma_table[light->current_brightness];
        else
            compare_val = ((uint32_t)light->current_brightness * max_arr) / 255;
    }

    uint32_t final_compare = light->inverted ? (max_arr - compare_val) : compare_val;

    __HAL_TIM_SET_COMPARE(light->htim, light->channel, final_compare);
}

void LIGHT_SetState(Light_t* light, bool state)
{
    if (!light) return;
    light->state = state;

    if (state)
    {
        // if turning ON and target was 0, restore full brightness
        if (light->target_brightness == 0)
            light->target_brightness = 255;
    }
    else
    {
        // if turning OFF, set target to 0 so LIGHT_ProcessFade smoothly ramps down
        light->target_brightness = 0;
    }
}

void LIGHT_SetBrightness(Light_t* light, uint8_t brightness)
{
    if (!light) return;
    light->target_brightness = brightness;
    light->state = (brightness > 0);
}

void LIGHT_ProcessFade(Light_t* light)
{
    if (!light) return;

    uint32_t now = HAL_GetTick();

    // if we are already at the target, keep the timer synced to prevent massive 
    // time-jumps when a new fade command finally arrives
    if (light->current_brightness == light->target_brightness) 
    {
        light->last_step_time = now;
        return;
    }

    uint32_t elapsed = now - light->last_step_time;

    if (elapsed >= light->fade_step_delay_ms)
    {
        // calculate how many steps we SHOULD have taken during this elapsed time
        uint32_t steps = elapsed / light->fade_step_delay_ms;
        
        // advance the timer by the exact time accounted for
        light->last_step_time += (steps * light->fade_step_delay_ms);

        if (light->current_brightness < light->target_brightness)
        {
            if ((uint32_t)light->current_brightness + steps >= light->target_brightness)
                light->current_brightness = light->target_brightness;
            else
                light->current_brightness += steps;
                
            LIGHT_UpdatePWM(light);
        }
        else if (light->current_brightness > light->target_brightness)
        {
            if ((uint32_t)light->current_brightness <= steps || 
                (uint32_t)light->current_brightness - steps <= light->target_brightness)
                light->current_brightness = light->target_brightness;
            else
                light->current_brightness -= steps;
                
            LIGHT_UpdatePWM(light);
        }
    }
}

void WIFIHANDLER_MQTT_PubDiscoveryLight(WIFI_t* wifi, Light_t* light)
{
    if (!wifi || !light || !light->light_id) return;

    char topic[128];
    char payload[WIFI_BUF_MAX_SIZE];

    snprintf(topic, sizeof(topic), "homeassistant/light/%s_%s/config", wifi->hostname, light->light_id);

    snprintf(payload, sizeof(payload), MQTT_DISCOVERY_LIGHT,
        light->friendly_name,
        wifi->hostname, light->light_id, // cmd_t
        wifi->hostname, light->light_id, // stat_t
        wifi->hostname, light->light_id, // bri_cmd_t
        wifi->hostname, light->light_id, // bri_stat_t
        wifi->hostname, light->light_id, // uniq_id
        wifi->hostname, wifi->name);     // dev

    WIFI_MQTT_Publish(wifi, topic, payload, 1, 1);
}

void WIFIHANDLER_MQTT_PubStateLight(WIFI_t* wifi, Light_t* light)
{
    if (!wifi || !light || !light->light_id) return;

    char topic[128];
    char payload[64];

    // state (1/0)
    snprintf(topic, sizeof(topic), MQTT_STATE_TOPIC_TEMPLATE, wifi->hostname, light->light_id);
    snprintf(payload, sizeof(payload), "%d", light->state ? 1 : 0);
    WIFI_MQTT_Publish(wifi, topic, payload, 0, 1);

    // current target brightness
    snprintf(topic, sizeof(topic), MQTT_BRI_STATE_TOPIC_TEMPLATE, wifi->hostname, light->light_id);
    snprintf(payload, sizeof(payload), "%d", light->target_brightness);
    WIFI_MQTT_Publish(wifi, topic, payload, 0, 1);
}

void WIFIHANDLER_MQTT_PubStateLightByID(WIFI_t* wifi, const char* light_id)
{
    Light_t* light = LIGHT_GetByID(light_id);
    if (light != NULL)
        WIFIHANDLER_MQTT_PubStateLight(wifi, light);
}

#endif

// ====================================================================
// SERVO FUNCTIONS
// ====================================================================
#ifdef ENABLE_SERVO
bool WIFIHANDLER_MQTT_RegisterServo(const char* servo_id, const char* friendly_name, 
    TIM_HandleTypeDef* htim, uint32_t channel, uint8_t min_angle, uint8_t max_angle, bool inverted)
{
    if (servo_count >= MAX_SERVOS || !servo_id)
        return false;

    uint32_t idx = servo_count;

#ifdef ENABLE_SAVE_TO_FLASH
    if (savedata.servo_min_angle[idx] > 180 || savedata.servo_max_angle[idx] > 180 ||
        savedata.servo_min_angle[idx] >= savedata.servo_max_angle[idx] ||
        savedata.servo_max_angle[idx] == 0 ||
        (savedata.servo_min_angle[idx] == 0xFF && savedata.servo_max_angle[idx] == 0xFF))
    {
        savedata.servo_min_angle[idx] = min_angle;
        savedata.servo_max_angle[idx] = max_angle;
        savedata.servo_angle[idx] = min_angle;
    }
    min_angle = savedata.servo_min_angle[idx];
    max_angle = savedata.servo_max_angle[idx];
#endif

    SERVO_Init(&servos[idx], servo_id, friendly_name, htim, channel, min_angle, max_angle, inverted ? 1 : 0);

#ifdef ENABLE_SAVE_TO_FLASH
    SERVO_SetAngle(&servos[idx], savedata.servo_angle[idx]);
#endif

    servo_count++;
    return true;
}

Servo_t* SERVO_GetByID(const char* servo_id)
{
    if (!servo_id) return NULL;
    for (uint32_t i = 0; i < servo_count; i++)
    {
        if (servos[i].servo_id != NULL && strcmp(servos[i].servo_id, servo_id) == 0)
            return &servos[i];
    }
    return NULL;
}

void WIFIHANDLER_MQTT_PubDiscoveryServo(WIFI_t* wifi, Servo_t* servo)
{
    if (!wifi || !servo || !servo->servo_id) return;

    char topic[128];
    char payload[WIFI_BUF_MAX_SIZE];

    // 1. Angle number entity
    snprintf(topic, sizeof(topic), "homeassistant/number/%s_%s/config", wifi->hostname, servo->servo_id);
    snprintf(payload, sizeof(payload), MQTT_DISCOVERY_SERVO,
        servo->name,
        wifi->hostname, servo->servo_id,
        wifi->hostname, servo->servo_id,
        (int)servo->min_angle, (int)servo->max_angle,
        wifi->hostname, servo->servo_id,
        wifi->hostname, wifi->name);
    WIFI_MQTT_Publish(wifi, topic, payload, 1, 1);

    // 2. Min angle limit entity
    snprintf(topic, sizeof(topic), "homeassistant/number/%s_%s_min/config", wifi->hostname, servo->servo_id);
    snprintf(payload, sizeof(payload), MQTT_DISCOVERY_SERVO_LIMIT,
        servo->name, "Min",
        wifi->hostname, servo->servo_id, "min",
        wifi->hostname, servo->servo_id, "min",
        wifi->hostname, servo->servo_id, "min",
        wifi->hostname, wifi->name);
    WIFI_MQTT_Publish(wifi, topic, payload, 1, 1);

    // 3. Max angle limit entity
    snprintf(topic, sizeof(topic), "homeassistant/number/%s_%s_max/config", wifi->hostname, servo->servo_id);
    snprintf(payload, sizeof(payload), MQTT_DISCOVERY_SERVO_LIMIT,
        servo->name, "Max",
        wifi->hostname, servo->servo_id, "max",
        wifi->hostname, servo->servo_id, "max",
        wifi->hostname, servo->servo_id, "max",
        wifi->hostname, wifi->name);
    WIFI_MQTT_Publish(wifi, topic, payload, 1, 1);
}

void WIFIHANDLER_MQTT_PubStateServo(WIFI_t* wifi, Servo_t* servo)
{
    if (!wifi || !servo || !servo->servo_id) return;

    char topic[128];
    char payload[16];

    // Angle state
    snprintf(topic, sizeof(topic), MQTT_STATE_TOPIC_TEMPLATE, wifi->hostname, servo->servo_id);
    snprintf(payload, sizeof(payload), "%u", servo->current_angle);
    WIFI_MQTT_Publish(wifi, topic, payload, 0, 1);

    // Min angle state
    snprintf(topic, sizeof(topic), "snse/%s/%s/min_state", wifi->hostname, servo->servo_id);
    snprintf(payload, sizeof(payload), "%u", servo->min_angle);
    WIFI_MQTT_Publish(wifi, topic, payload, 0, 1);

    // Max angle state
    snprintf(topic, sizeof(topic), "snse/%s/%s/max_state", wifi->hostname, servo->servo_id);
    snprintf(payload, sizeof(payload), "%u", servo->max_angle);
    WIFI_MQTT_Publish(wifi, topic, payload, 0, 1);
}

void WIFIHANDLER_MQTT_PubStateServoByID(WIFI_t* wifi, const char* servo_id)
{
    Servo_t* servo = SERVO_GetByID(servo_id);
    if (servo != NULL)
        WIFIHANDLER_MQTT_PubStateServo(wifi, servo);
}
#endif

// ====================================================================
// MQTT FUNCTIONS
// ====================================================================

void WIFIHANDLER_MQTT_PublishAllDiscovery(WIFI_t* wifi)
{
	// switches
	for (uint32_t i = 0; i < switch_count; i++)
		WIFIHANDLER_MQTT_PubDiscoverySwitch(wifi, &switches[i]);

	// sensors
	for (uint32_t i = 0; i < sensor_count; i++)
		WIFIHANDLER_MQTT_PubDiscoverySensor(wifi, &sensors[i]);

	// covers
#ifdef ENABLE_COVER
    for (uint32_t i = 0; i < cover_count; i++)
        WIFIHANDLER_MQTT_PubDiscoveryCover(wifi, &covers[i]);
#endif

	// PWM lights
#ifdef ENABLE_LIGHT
    for (uint32_t i = 0; i < light_count; i++)
        WIFIHANDLER_MQTT_PubDiscoveryLight(wifi, &lights[i]);
#endif

	// Servos
#ifdef ENABLE_SERVO
    for (uint32_t i = 0; i < servo_count; i++)
        WIFIHANDLER_MQTT_PubDiscoveryServo(wifi, &servos[i]);
#endif

    USER_MQTT_CustomDiscovery(wifi);
}

void WIFIHANDLER_MQTT_PublishAllStates(WIFI_t* wifi)
{
	// switches
	for (uint32_t i = 0; i < switch_count; i++)
		WIFIHANDLER_MQTT_PubStateSwitch(wifi, &switches[i]);

	// sensors
	for (uint32_t i = 0; i < sensor_count; i++)
		WIFIHANDLER_MQTT_PubStateSensor(wifi, &sensors[i]);

	// covers
#ifdef ENABLE_COVER
    for (uint32_t i = 0; i < cover_count; i++)
        WIFIHANDLER_MQTT_PubStateCover(wifi, &covers[i]);
#endif

	// PWM lights
#ifdef ENABLE_LIGHT
    for (uint32_t i = 0; i < light_count; i++)
        WIFIHANDLER_MQTT_PubStateLight(wifi, &lights[i]);
#endif

	// Servos
#ifdef ENABLE_SERVO
    for (uint32_t i = 0; i < servo_count; i++)
        WIFIHANDLER_MQTT_PubStateServo(wifi, &servos[i]);
#endif

    USER_MQTT_CustomPublishStates(wifi);
}

void WIFIHANDLER_MQTT_SendNotification(WIFI_t* wifi, const char* message)
{
    char topic[128];
    snprintf(topic, sizeof(topic), "snse/%s/notify", wifi->hostname);
    WIFI_MQTT_Publish(wifi, topic, message, 1, 0);
}

void WIFIHANDLER_MQTT_Loop(WIFI_t* wifi)
{
	char topic_in[128];
	char payload_in[64];

#ifdef ENABLE_COVER
    for (uint32_t i = 0; i < cover_count; i++)
    {
        Cover_t* cov = &covers[i];
        if (cov->stepper->state == STEPPER_STOPPED)
        {
            // If it was previously moving, publish final position & state
            if (cov->last_state != STEPPER_STOPPED)
            {
                cov->last_state = STEPPER_STOPPED;
                WIFIHANDLER_MQTT_PubStateCover(wifi, cov);
            }
            DRV8825_Sleep(cov->stepper);
        }
        else
            cov->last_state = cov->stepper->state;
    }
#endif

	while (WIFI_MQTT_Receive(wifi, topic_in, payload_in, 0) == OK)
	{
		char expected_topic[128];
		
		// ====================================================================
		// RECEIVE SWITCH ON/OFF COMMANDS BY ID
		// ====================================================================
		for (uint32_t i = 0; i < switch_count; i++)
		{
			snprintf(expected_topic, sizeof(expected_topic), MQTT_SWITCH_CMD_TEMPLATE, wifi->hostname, switches[i].switch_id);
			if (strcmp(topic_in, expected_topic) == 0)
			{
				Switch_t* sw = &switches[i];
				if (strcmp(payload_in, "1") == 0)
				{
					SWITCH_Press(sw);
					sw->manual = true;
				}
				else if (strcmp(payload_in, "0") == 0)
				{
					SWITCH_UnPress(sw);
					sw->manual = false;
				}

				WIFIHANDLER_MQTT_PubStateSwitch(wifi, sw);
				break;
			}
		}

		// ====================================================================
		// RECEIVE COVER COMMANDS (OPEN / CLOSE / STOP) AND TARGET POSITION
		// ====================================================================
#ifdef ENABLE_COVER
		for (uint32_t i = 0; i < cover_count; i++)
		{
			Cover_t* cov = &covers[i];

			// command handling (OPEN / CLOSE / STOP)
			snprintf(expected_topic, sizeof(expected_topic), "snse/%s/%s/set", wifi->hostname, cov->cover_id);
			if (strcmp(topic_in, expected_topic) == 0)
			{
				if (strcmp(payload_in, "OPEN") == 0)        COVER_Open(cov);
				else if (strcmp(payload_in, "CLOSE") == 0)  COVER_Close(cov);
				else if (strcmp(payload_in, "STOP") == 0)   COVER_Stop(cov);
				WIFIHANDLER_MQTT_PubStateCover(wifi, cov);
				break;
			}

			// target position handling (0 - 100%)
			snprintf(expected_topic, sizeof(expected_topic), "snse/%s/%s/pos_set", wifi->hostname, cov->cover_id);
			if (strcmp(topic_in, expected_topic) == 0)
			{
				uint8_t percent = (uint8_t)atoi(payload_in);
				COVER_SetPosition(cov, percent);
				WIFIHANDLER_MQTT_PubStateCover(wifi, cov);
				break;
			}
		}
#endif

		// ====================================================================
		// RECEIVE LED ON/OFF OR BRIGHTNESS CONTROL
		// ====================================================================
#ifdef ENABLE_LIGHT
        for (uint32_t i = 0; i < light_count; i++)
        {
            Light_t* lit = &lights[i];

            // ON/OFF command
            snprintf(expected_topic, sizeof(expected_topic), "snse/%s/%s/set", wifi->hostname, lit->light_id);
            if (strcmp(topic_in, expected_topic) == 0)
            {
                if (strcmp(payload_in, "1") == 0)
                    LIGHT_SetState(lit, true);
                else if (strcmp(payload_in, "0") == 0)
                    LIGHT_SetState(lit, false);
                WIFIHANDLER_MQTT_PubStateLight(wifi, lit);
                break;
            }

            // brightness command (0 - 255)
            snprintf(expected_topic, sizeof(expected_topic), "snse/%s/%s/bri_set", wifi->hostname, lit->light_id);
            if (strcmp(topic_in, expected_topic) == 0)
            {
                uint8_t bri = (uint8_t)atoi(payload_in);
                LIGHT_SetBrightness(lit, bri);
                
                // publish state back immediately so HA slider updates instantly
                WIFIHANDLER_MQTT_PubStateLight(wifi, lit);
                break;
            }
        }
#endif

		// ====================================================================
		// RECEIVE SERVO COMMANDS (ANGLE, MIN ANGLE, MAX ANGLE)
		// ====================================================================
#ifdef ENABLE_SERVO
        for (uint32_t i = 0; i < servo_count; i++)
        {
            Servo_t* srv = &servos[i];

            // Angle command
            snprintf(expected_topic, sizeof(expected_topic), "snse/%s/%s/set", wifi->hostname, srv->servo_id);
            if (strcmp(topic_in, expected_topic) == 0)
            {
                if (strcmp(payload_in, "OPEN") == 0)
                {
                    SERVO_SetAngle(srv, srv->max_angle);
                }
                else if (strcmp(payload_in, "CLOSE") == 0)
                {
                    SERVO_SetAngle(srv, srv->min_angle);
                }
                else
                {
                    int angle = atoi(payload_in);
                    if (angle >= 0 && angle <= 255)
                        SERVO_SetAngle(srv, (uint8_t)angle);
                }

#ifdef ENABLE_SAVE_TO_FLASH
                savedata.servo_angle[i] = srv->current_angle;
#endif
                WIFIHANDLER_MQTT_PubStateServo(wifi, srv);
                break;
            }

            // Min angle command
            snprintf(expected_topic, sizeof(expected_topic), "snse/%s/%s/min_set", wifi->hostname, srv->servo_id);
            if (strcmp(topic_in, expected_topic) == 0)
            {
                int min_a = atoi(payload_in);
                if (min_a >= 0 && min_a <= 180)
                {
                    SERVO_SetMinAngle(srv, (uint8_t)min_a);
#ifdef ENABLE_SAVE_TO_FLASH
                    savedata.servo_min_angle[i] = srv->min_angle;
                    savedata.servo_angle[i] = srv->current_angle;
                    FLASH_WriteSaveData();
#endif
                    WIFIHANDLER_MQTT_PubDiscoveryServo(wifi, srv);
                    WIFIHANDLER_MQTT_PubStateServo(wifi, srv);
                }
                break;
            }

            // Max angle command
            snprintf(expected_topic, sizeof(expected_topic), "snse/%s/%s/max_set", wifi->hostname, srv->servo_id);
            if (strcmp(topic_in, expected_topic) == 0)
            {
                int max_a = atoi(payload_in);
                if (max_a >= 0 && max_a <= 180)
                {
                    SERVO_SetMaxAngle(srv, (uint8_t)max_a);
#ifdef ENABLE_SAVE_TO_FLASH
                    savedata.servo_max_angle[i] = srv->max_angle;
                    savedata.servo_angle[i] = srv->current_angle;
                    FLASH_WriteSaveData();
#endif
                    WIFIHANDLER_MQTT_PubDiscoveryServo(wifi, srv);
                    WIFIHANDLER_MQTT_PubStateServo(wifi, srv);
                }
                break;
            }
        }
#endif

        // resend discovery when HA restarts
        if (strcmp(topic_in, "homeassistant/status") == 0)
        {
            if (strcmp(payload_in, "online") == 0)
            {
                WIFIHANDLER_MQTT_PublishAllDiscovery(wifi);
                WIFIHANDLER_MQTT_PublishAllStates(wifi);
            }
        }

        USER_MQTT_CustomHandler(wifi, topic_in, payload_in);
	}
}