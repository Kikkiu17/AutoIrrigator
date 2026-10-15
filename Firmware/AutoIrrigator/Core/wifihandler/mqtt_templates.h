#ifndef MQTT_TEMPLATES_H_
#define MQTT_TEMPLATES_H_

/* 
 * SWITCH
 * Use for relays, valves, or anything that can be toggled On/Off.
 * Topic: homeassistant/switch/<uniq_id>/config
 *
 * Parameters:
 * 1. Friendly Name (e.g., "Main Relay")
 * 2. Command Topic (e.g., "snse/device_name/switch1/set")
 * 3. State Topic (e.g., "snse/device_name/switch1/state")
 * 4. Unique ID (e.g., "device_name_switch1")
 * 5. Device ID (e.g., "device_name")
 * 6. Device Friendly Name (e.g., "My SNSE Device")
 */
static const char MQTT_DISCOVERY_SWITCH[] = 
"{\"name\":\"%s\",\"cmd_t\":\"%s\",\"stat_t\":\"%s\",\"pl_on\":\"1\",\"pl_off\":\"0\",\"uniq_id\":\"%s\",\"dev\":{\"ids\":[\"%s\"],\"name\":\"%s\"}}";

/* 
 * SENSOR
 * Use for read-only numeric or text values (e.g., Voltage, Temperature).
 * Topic: homeassistant/sensor/<uniq_id>/config
 *
 * Parameters:
 * 1. Friendly Name (e.g., "Voltage")
 * 2. State Topic (e.g., "snse/device_name/voltage/state")
 * 3. Unit of Measurement (e.g., "V", "°C", "%". Leave empty if none)
 * 4. Device Class (e.g., "voltage", "temperature", "battery", "power")
 * 5. Unique ID (e.g., "device_name_voltage")
 * 6. Device ID (e.g., "device_name")
 * 7. Device Friendly Name (e.g., "My SNSE Device")
 */
static const char MQTT_DISCOVERY_SENSOR[] = 
"{\"name\":\"%s\",\"stat_t\":\"%s\",\"unit_of_meas\":\"%s\",\"dev_cla\":\"%s\",\"stat_cla\":\"measurement\",\"uniq_id\":\"%s\",\"dev\":{\"ids\":[\"%s\"],\"name\":\"%s\"}}";

/* 
 * TEXT INPUT / TIME PICKER
 * Use for reading/writing string data (e.g., schedules like "08:00-12:00").
 * Topic: homeassistant/text/<uniq_id>/config
 *
 * Parameters:
 * 1. Friendly Name (e.g., "Schedule")
 * 2. Command Topic (e.g., "snse/device_name/schedule/set")
 * 3. State Topic (e.g., "snse/device_name/schedule/state")
 * 4. Unique ID (e.g., "device_name_schedule")
 * 5. Device ID (e.g., "device_name")
 * 6. Device Friendly Name (e.g., "My SNSE Device")
 */
static const char MQTT_DISCOVERY_TEXT[] = 
"{\"name\":\"%s\",\"cmd_t\":\"%s\",\"stat_t\":\"%s\",\"uniq_id\":\"%s\",\"dev\":{\"ids\":[\"%s\"],\"name\":\"%s\"}}";

/* 
 * BUTTON
 * Use for triggering a single event without holding a state (e.g., "Reset", "Reboot").
 * Topic: homeassistant/button/<uniq_id>/config
 *
 * Parameters:
 * 1. Friendly Name (e.g., "Reboot Device")
 * 2. Command Topic (e.g., "snse/device_name/button_reboot/set")
 * 3. Payload Press (e.g., "1" or "PRESS")
 * 4. Unique ID (e.g., "device_name_btn_reboot")
 * 5. Device ID (e.g., "device_name")
 * 6. Device Friendly Name (e.g., "My SNSE Device")
 */
static const char MQTT_DISCOVERY_BUTTON[] = 
"{\"name\":\"%s\",\"cmd_t\":\"%s\",\"pl_prs\":\"%s\",\"uniq_id\":\"%s\",\"dev\":{\"ids\":[\"%s\"],\"name\":\"%s\"}}";

/* 
 * BINARY SENSOR
 * Use for read-only boolean states (e.g., Motion detected, Door opened).
 * Topic: homeassistant/binary_sensor/<uniq_id>/config
 *
 * Parameters:
 * 1. Friendly Name (e.g., "Front Door")
 * 2. State Topic (e.g., "snse/device_name/door/state")
 * 3. Payload On (e.g., "1")
 * 4. Payload Off (e.g., "0")
 * 5. Device Class (e.g., "door", "motion", "presence")
 * 6. Unique ID (e.g., "device_name_door")
 * 7. Device ID (e.g., "device_name")
 * 8. Device Friendly Name (e.g., "My SNSE Device")
 */
static const char MQTT_DISCOVERY_BINARY_SENSOR[] = 
"{\"name\":\"%s\",\"stat_t\":\"%s\",\"pl_on\":\"%s\",\"pl_off\":\"%s\",\"dev_cla\":\"%s\",\"uniq_id\":\"%s\",\"dev\":{\"ids\":[\"%s\"],\"name\":\"%s\"}}";

/* 
 * LIGHT
 * Topic: homeassistant/light/<uniq_id>/config
 * Parameters:
 * 1. Friendly Name
 * 2. Command Topic
 * 3. State Topic
 * 4. Brightness Command Topic
 * 5. Brightness State Topic
 * 6. Unique ID
 * 7. Device ID
 * 8. Device Friendly Name
 */
static const char MQTT_DISCOVERY_LIGHT[] = 
"{\"name\":\"%s\",\"cmd_t\":\"snse/%s/%s/set\",\"stat_t\":\"snse/%s/%s/state\",\"bri_cmd_t\":\"snse/%s/%s/bri_set\",\"bri_stat_t\":\"snse/%s/%s/bri_state\",\"bri_scl\":255,\"pl_on\":\"1\",\"pl_off\":\"0\",\"uniq_id\":\"%s_%s\",\"dev\":{\"ids\":[\"%s\"],\"name\":\"%s\"}}";

static const char MQTT_DISCOVERY_COVER[] =
"{\"name\":\"%s\",\"cmd_t\":\"snse/%s/%s/set\",\"stat_t\":\"snse/%s/%s/state\",\"pos_t\":\"snse/%s/%s/position\",\"set_pos_t\":\"snse/%s/%s/pos_set\",\"pl_open\":\"OPEN\",\"pl_close\":\"CLOSE\",\"pl_stop\":\"STOP\",\"uniq_id\":\"%s_%s\",\"dev\":{\"ids\":[\"%s\"],\"name\":\"%s\"}}";

static const char MQTT_DISCOVERY_SERVO[] = 
"{\"name\":\"%s\",\"cmd_t\":\"snse/%s/%s/set\",\"stat_t\":\"snse/%s/%s/state\",\"min\":%d,\"max\":%d,\"step\":1,\"mode\":\"slider\",\"unit_of_meas\":\"°\",\"uniq_id\":\"%s_%s\",\"dev\":{\"ids\":[\"%s\"],\"name\":\"%s\"}}";

static const char MQTT_DISCOVERY_SERVO_LIMIT[] = 
"{\"name\":\"%s Angolo %s\",\"cmd_t\":\"snse/%s/%s/%s_set\",\"stat_t\":\"snse/%s/%s/%s_state\",\"min\":0,\"max\":180,\"step\":1,\"mode\":\"box\",\"unit_of_meas\":\"°\",\"uniq_id\":\"%s_%s_%s\",\"dev\":{\"ids\":[\"%s\"],\"name\":\"%s\"}}";



// homeassistant/sensor/<unique_id>/config
// which is: homeassistant/sensor/<hostname>_<sensor_type>/config
static const char MQTT_DISCOVERY_SENSOR_TOPIC_TEMPLATE[] =
"homeassistant/sensor/%s_%s/config";

// snse/hostname/<sensor_type>/state
static const char MQTT_SENSOR_STATE_TEMPLATE[] =
"snse/%s/%s/state";

// <hostname>_<sensor_type>
static const char MQTT_SENSOR_UNIQUE_ID_TEMPLATE[] =
"%s_%s";

/// homeassistant/switch/<hostname>_<switch_id>/config
static const char MQTT_DISCOVERY_SWITCH_TOPIC_TEMPLATE[] =
"homeassistant/switch/%s_%s/config";

// snse/<hostname>/<switch_id>/set
static const char MQTT_SWITCH_CMD_TEMPLATE[] =
"snse/%s/%s/set";

// snse/<hostname>/<switch_id>/state
static const char MQTT_SWITCH_STATE_TEMPLATE[] =
"snse/%s/%s/state";

// <hostname>_<switch_id>
static const char MQTT_SWITCH_UNIQUE_ID_TEMPLATE[] =
"%s_%s";

static const char MQTT_STATE_SWITCH_TOPIC_TEMPLATE[] =
"snse/%s/%s/state";

static const char MQTT_STATE_SENSOR_TOPIC_TEMPLATE[] =
"snse/%s/%s/state";

// State templates unified for all components
static const char MQTT_STATE_TOPIC_TEMPLATE[] = "snse/%s/%s/state";
static const char MQTT_POSITION_TOPIC_TEMPLATE[] = "snse/%s/%s/position";
static const char MQTT_BRI_STATE_TOPIC_TEMPLATE[] = "snse/%s/%s/bri_state";

/* 
 * TIME PICKER
 * Parameters: devicename, hostname, valve_id, type(start/end), hostname, valve_id, type,
 * hostname, valve_id, type, hostname, devicename
 */
static const char MQTT_DISCOVERY_TIME[] = 
"{\"name\":\"%s\",\"cmd_t\":\"snse/%s/valve%d/start/set\",\"stat_t\":\"snse/%s/valve%d/start/state\",\"uniq_id\":\"%s_valve%d_start\",\"dev\":{\"ids\":[\"%s\"],\"name\":\"%s\"}}";

/* 
 * DURATION PICKER
 * Parameters: name, hostname, valve_id, hostname, valve_id, hostname, valve_id, hostname, devicename
 */
static const char MQTT_DISCOVERY_DURATION[] = 
"{\"name\":\"%s\",\"cmd_t\":\"snse/%s/valve%d/duration/set\",\"stat_t\":\"snse/%s/valve%d/duration/state\",\"max\":720,\"unit_of_meas\":\"min\",\"uniq_id\":\"%s_valve%d_duration\",\"dev\":{\"ids\":[\"%s\"],\"name\":\"%s\"}}";

#endif /* MQTT_TEMPLATES_H_ */

