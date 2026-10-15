/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dma.h"
#include "stm32g0xx_hal_def.h"
#include "stm32g0xx_hal_tim.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdio.h>

#include "../ESP8266/esp8266.h"
#include "../wifihandler/wifihandler.h"
#include "../wifihandler/userhandlers.h"
#include "../credentials.h"
#include "../irrigator/irrigator.h"
#include "../settings.h"
#include "../weather/weather.h"
#include "servo.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
Battery_t bat;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
#define BAT_ADC_CALIBRATION_VALUE 2128 / 600
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
const uint32_t MAX_FLOW_PERIOD = 1 / ((float)MIN_WATER_FLOW / 60.0 * 7.5) * 1000;

uint8_t time_hour = 0xFF;
uint8_t time_minute = 0xFF;

WIFI_t wifi;

Valve_t valve_list[VALVES_NUM];
Flow_t flow1;
Flow_t flow2;
Flow_t flow3;
Flow_t flow4;
Schedule_t schedule1;
Schedule_t schedule2;
Schedule_t schedule3;
Schedule_t schedule4;

Weather_t weather;
int8_t yesterday_last_precipitation_hour = -1;
Switch_t* sw_valves[VALVES_NUM];
bool last_sw_status[VALVES_NUM] = {false, false, false, false};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void BATTERY_GetVoltage(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static bool is_rain_blocking(void)
{
  if (weather.current_precipitation >= PRECIPITATION_THRESHOLD)
    return true;
  if (time_hour <= 23)
  {
    if (yesterday_last_precipitation_hour >= 0 && (24 - yesterday_last_precipitation_hour + time_hour <= 12))
      return true;

    uint8_t start_h = (time_hour > 12) ? (time_hour - 12) : 0;
    uint8_t end_h = (time_hour + 12 < 24) ? (time_hour + 12) : 23;
    for (uint8_t h = start_h; h <= end_h; h++)
    {
      if (weather.hourly_precipitation[h] >= PRECIPITATION_THRESHOLD)
        return true;
    }
  }
  return false;
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_TIM1_Init();
  MX_TIM3_Init();
  MX_TIM14_Init();
  MX_USART1_UART_Init();
  MX_ADC1_Init();
  /* USER CODE BEGIN 2 */
  // ESPRST is HIGH by default (set up in .ioc file) so ESP is enabled by default
  if (ESP8266_Init() == TIMEOUT)
  {
	while (1)
		__NOP();
  }

  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4);

#ifdef ENABLE_SAVE_TO_FLASH
  FLASH_ReadSaveData();
  if (WIFI_SetName(&wifi, savedata.name) == ERR)
    WIFI_SetName(&wifi, (char*)ESP_NAME); // happens when there is nothing saved to FLASH, so set default name
  WIFI_SetIP(&wifi, savedata.ip);         // if there is nothing saved to FLASH, this function does nothing
#else
  WIFI_SetName(&wifi, (char*)ESP_NAME);
#endif

  memcpy(wifi.SSID, ssid, strlen(ssid));
  memcpy(wifi.pw, password, strlen(password));
  HAL_GPIO_WritePin(STATUS_Port, STATUS_Pin, 1);
  uint32_t connect_status = WIFI_Connect(&wifi);
  if (connect_status == FAIL || connect_status == ERROR)
  {
    // try again
    NVIC_SystemReset();
  }
  HAL_GPIO_WritePin(STATUS_Port, STATUS_Pin, 0);
  WIFI_EnableNTPServer(&wifi, 0);

  /*
  The first time the ESP connects to WiFi, the gateway assigns an IP to it, which now gets saved to FLASH.
  The next time the ESP connects, the gateway could assign a different IP; to prevent this, the function
  WIFI_SetIP(&wifi, savedata.ip); loads the IP previously saved on FLASH so that the ESP tries to connect
  and get this IP
  */
  strncpy(savedata.ip, wifi.IP, 15);
  FLASH_WriteSaveData();

  HAL_TIM_IC_Start_IT(&htim14, TIM_CHANNEL_1);
  HAL_TIM_IC_Start_IT(&htim3, TIM_CHANNEL_4);
  HAL_TIM_IC_Start_IT(&htim3, TIM_CHANNEL_1);
  HAL_TIM_IC_Start_IT(&htim3, TIM_CHANNEL_2);

  static const char* const valve_names[VALVES_NUM] = {"Valvola Ovest", "Valvola Sud", "Valvola Sud-Est", "Valvola Est"};
  static const char* const valve_ids[VALVES_NUM] = {"valve1", "valve2", "valve3", "valve4"};
  static const char* const flow_names[VALVES_NUM] = {"Flusso Ovest", "Flusso Sud", "Flusso Sud-Est", "Flusso totale"};
  static const char* const flow_ids[VALVES_NUM] = {"flow1", "flow2", "flow3", "total_flow"};
  GPIO_TypeDef* const ports[VALVES_NUM] = {VALVE1_GPIO_Port, VALVE2_GPIO_Port, VALVE3_GPIO_Port, VALVE4_GPIO_Port};
  const uint16_t pins[VALVES_NUM] = {VALVE1_Pin, VALVE2_Pin, VALVE3_Pin, VALVE4_Pin};
  Flow_t* const flows[VALVES_NUM] = {&flow1, &flow2, &flow3, &flow4};
  Schedule_t* const scheds[VALVES_NUM] = {&schedule1, &schedule2, &schedule3, &schedule4};
  int32_t* const flow_vals[VALVES_NUM] = {&flow1_val, &flow2_val, &flow3_val, &total_flow_val};

  for (int i = 0; i < VALVES_NUM; i++)
  {
    WIFIHANDLER_MQTT_RegisterSwitch(valve_ids[i], valve_names[i], false, ports[i], pins[i], false);
    sw_valves[i] = &switches[i];
    VALVE_Init(&valve_list[i], flows[i], scheds[i], i + 1, ports[i], pins[i], sw_valves[i]);
    WIFIHANDLER_MQTT_RegisterSensor(flow_names[i], "L/h", flow_ids[i], "volume_flow_rate", flow_vals[i], 0);
  }
  WIFIHANDLER_MQTT_RegisterSensor("Tensione batteria", "V", "battery", "voltage", &bat_voltage_val, 2);

#ifdef ENABLE_SERVO
  WIFIHANDLER_MQTT_RegisterServo("servo1", "Servo irrigatore", &htim1, TIM_CHANNEL_4, SERVO_DEFAULT_MIN_ANGLE, SERVO_DEFAULT_MAX_ANGLE, true);
#endif

  WEATHER_GetForecast(&weather, ESP8266_GetBuffer());
  WIFI_GetTime(&wifi);
  int32_t init_h = bufferToInt(wifi.time, 2);
  int32_t init_m = bufferToInt(wifi.time + 3, 2);
  if (init_h >= 0 && init_h <= 23) time_hour = (uint8_t)init_h;
  if (init_m >= 0 && init_m <= 59) time_minute = (uint8_t)init_m;
  rain_factor_active = is_rain_blocking();

  SCHEDULE_ReadFromFlash(valve_list, VALVES_NUM);

  HAL_ADCEx_Calibration_Start(&hadc1);

  if (connect_status == OK)
  {
    if (WIFIHANDLER_MQTT_Init(&wifi, MQTT_BROKER_IP, MQTT_BROKER_PORT) == OK) 
    {
      WIFIHANDLER_MQTT_PublishAllDiscovery(&wifi);
      WIFIHANDLER_MQTT_PublishAllStates(&wifi);
    }
  }
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  uint32_t timestamp = 0;
  uint32_t last_mqtt_publish = 0;

  //bool low_bat_notification = false;
  bool valves_closed_notification = false;

  while (1)
  {
    BATTERY_GetVoltage();
    USER_CalculateSensors();

    // Synchronize valve states with switch states (if MQTT received a command)
    for (uint32_t i = 0; i < VALVES_NUM; i++)
    {
      if (valve_list[i].sw != NULL)
      {
        valve_list[i].isOpen = valve_list[i].sw->pressed;
        valve_list[i].has_manual_override = valve_list[i].sw->manual;
      }
    }

    HAL_GPIO_WritePin(STATUS_Port, STATUS_Pin, 1);
    WIFIHANDLER_MQTT_Loop(&wifi);
    
    WIFIHANDLER_ReconnectIfDisconnected(&wifi);
    
    if (HAL_GetTick() - last_mqtt_publish > MQTT_PUBLISH_INTERVAL) 
    {
      last_mqtt_publish = HAL_GetTick();
      USER_PublishSensors(&wifi);
      for (int i = 0; i < VALVES_NUM; i++)
      {
        if (sw_valves[i] != NULL && sw_valves[i]->pressed != last_sw_status[i])
        {
          WIFIHANDLER_MQTT_PubStateSwitch(&wifi, sw_valves[i]);
          last_sw_status[i] = sw_valves[i]->pressed;
        }
      }
    }
    HAL_GPIO_WritePin(STATUS_Port, STATUS_Pin, 0);

    // HANDLE WATER FLOW
    // check flow for each valve
    for (uint32_t i = 0; i < VALVES_NUM; i++)
    {
      Valve_t* valve = &(valve_list[i]);
      if (valve->flow == NULL) continue;
      if (uwTick - valve->flow->ic_timestamp > MAX_FLOW_PERIOD)
        valve->flow->lt_per_hour = 0;
    }

    // check time periodically and handle schedule every minute
    if (uwTick - timestamp >= 1000)
    {
      timestamp = uwTick;
      int32_t cur_h = WIFI_GetTimeHour(&wifi);
      int32_t cur_m = WIFI_GetTimeMinutes(&wifi);
      if (cur_h >= 0 && cur_h <= 23 && cur_m >= 0 && cur_m <= 59)
      {
        if (cur_m != time_minute || cur_h != time_hour)
        {
          time_hour = (uint8_t)cur_h;
          time_minute = (uint8_t)cur_m;

          // --------- UPDATE FORECAST ---------
          if (time_minute % 15 == 0)
            WEATHER_GetForecast(&weather, ESP8266_GetBuffer());

          if (time_hour == 23 && time_minute == 59)
            yesterday_last_precipitation_hour = WEATHER_GetTodayLastPrecipitation(&weather);

          // --------- KEEP VALVES CLOSED IF RAIN IS A FACTOR ---------
          bool keep_valves_closed = is_rain_blocking();
          if (keep_valves_closed != rain_factor_active)
          {
            USER_MQTT_PublishRainFactor(&wifi, keep_valves_closed);
          }

          if (keep_valves_closed && !valves_closed_notification)
          {
            valves_closed_notification = true;
            WIFIHANDLER_MQTT_SendNotification(&wifi, NOTIFICATION_WEATHER_NO_VALVE_OPEN);
          }
          else if (!keep_valves_closed)
          {
            valves_closed_notification = false;
          }

          // --------- CLOSE/OPEN VALVES ---------
          for (uint32_t i = 0; i < VALVES_NUM; i++)
          {
            // code executed for each valve
            Valve_t* valve = &(valve_list[i]);

            if (keep_valves_closed)
            {
              // executed when rain is last/next 12 hours
              if (!valve->has_manual_override)
              {
                VALVE_Close(valve);
                if (valve->sw != NULL)
                {
                  WIFIHANDLER_MQTT_PubStateSwitch(&wifi, valve->sw);
                  last_sw_status[i] = valve->sw->pressed;
                }
              }
            }
            else
            {
              // executed in normal conditions (no rain)
              // open the valve following schedule only if it doesn't rain and duration > 0
              if (valve->schedule->duration > 0 &&
                  time_hour == valve->schedule->hour_open && time_minute == valve->schedule->minute_open)
              {
                valve->has_manual_override = false;
                if (valve->sw != NULL) valve->sw->manual = false;
                VALVE_Open(valve);
                if (valve->sw != NULL)
                {
                  WIFIHANDLER_MQTT_PubStateSwitch(&wifi, valve->sw);
                  last_sw_status[i] = valve->sw->pressed;
                }
              }
            }

            // close valve anyway (following schedule), regardless of rain/manual override
            if (valve->schedule->duration > 0)
            {
              int32_t close_minute = ((int32_t)valve->schedule->hour_open * 60 + valve->schedule->minute_open + valve->schedule->duration) % 1440;
              if ((time_hour * 60 + time_minute) == close_minute)
              {
                valve->has_manual_override = false;
                if (valve->sw != NULL) valve->sw->manual = false;
                VALVE_Close(valve);
                if (valve->sw != NULL)
                {
                  WIFIHANDLER_MQTT_PubStateSwitch(&wifi, valve->sw);
                  last_sw_status[i] = valve->sw->pressed;
                }
              }
            }
          }
        }
      }
    }

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV1;
  RCC_OscInitStruct.PLL.PLLN = 8;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
void BATTERY_GetVoltage(void)
{
	HAL_ADC_Start(&hadc1);
	HAL_ADC_PollForConversion(&hadc1, 250);
	bat.voltage_mv = HAL_ADC_GetValue(&hadc1) * BAT_ADC_CALIBRATION_VALUE;
	bat.voltage_integer = bat.voltage_mv / 1000;
	bat.voltage_decimal = (bat.voltage_mv - bat.voltage_integer * 1000) / 10;
}

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
	if (htim->Instance == TIM14 && htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1)
	{
		// FLOW1, VALVE1
		// TIM14 only has one channel
		uint32_t val = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);
		valve_list[0].flow->ic_val = val;
		FLOW_CalculateFlow(valve_list[0].flow);
	}
	else if (htim->Instance == TIM3 && htim->Channel)
	{
		if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1)
		{
			// FLOW4
			uint32_t val = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);
			valve_list[3].flow->ic_val = val;
			FLOW_CalculateFlow(valve_list[3].flow);
		}
		else if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2)
		{
			// FLOW3
			uint32_t val = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_2);
			valve_list[2].flow->ic_val = val;
			FLOW_CalculateFlow(valve_list[2].flow);
		}
    else if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_4)
    {
      // FLOW2
      uint32_t val = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);
      valve_list[1].flow->ic_val = val;
      FLOW_CalculateFlow(valve_list[1].flow);
    }
	}
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
