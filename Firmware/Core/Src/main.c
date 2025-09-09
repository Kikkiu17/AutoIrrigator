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
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdio.h>

#include "../ESP8266/esp8266.h"
#include "../wifihandler/wifihandler.h"
#include "../credentials.h"
#include "../irrigator/irrigator.h"
#include "../settings.h"
#include "../weather/weather.h"
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
#define BAT_ADC_CALIBRATION_VALUE 36 / 10
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
const uint32_t MAX_FLOW_PERIOD = 1 / ((float)MIN_WATER_FLOW / 60.0 * 7.5) * 1000;

uint8_t time_hour;
uint8_t time_minute;

WIFI_t wifi;
Connection_t conn;

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
int8_t yesterday_last_precipitation_hour;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void BATTERY_GetVoltage(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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
		  __asm__("nop");
  }


  memcpy(wifi.SSID, ssid, strlen(ssid));
  memcpy(wifi.pw, password, strlen(password));
  WIFI_Connect(&wifi);
  //WIFI_SetIP(&wifi, (char*)ESP_IP);
  //WIFI_SetHostname(&wifi, (char*)ESP_HOSTNAME);
  WIFI_SetName(&wifi, (char*)ESP_NAME);
  WIFI_EnableNTPServer(&wifi, 2);

  HAL_GPIO_TogglePin(STATUS_GPIO_Port, STATUS_Pin);
  WIFI_StartServer(&wifi, SERVER_PORT);
  HAL_GPIO_TogglePin(STATUS_GPIO_Port, STATUS_Pin);

  HAL_TIM_IC_Start_IT(&htim14, TIM_CHANNEL_1);
  HAL_TIM_IC_Start_IT(&htim1, TIM_CHANNEL_1);
  HAL_TIM_IC_Start_IT(&htim3, TIM_CHANNEL_1);
  HAL_TIM_IC_Start_IT(&htim3, TIM_CHANNEL_2);

  VALVE_Init(&valve_list[0], &flow1, &schedule1, 1, VALVE1_GPIO_Port, VALVE1_Pin);
  VALVE_Init(&valve_list[1], &flow2, &schedule2, 2, VALVE2_GPIO_Port, VALVE2_Pin);
  VALVE_Init(&valve_list[2], &flow3, &schedule3, 3, VALVE3_GPIO_Port, VALVE3_Pin);
  VALVE_Init(&valve_list[3], &flow4, &schedule4, 4, VALVE4_GPIO_Port, VALVE4_Pin);

  WEATHER_GetForecast(&weather, ESP8266_GetBuffer());
  SCHEDULE_ReadFromFlash(valve_list, VALVES_NUM);

  HAL_ADCEx_Calibration_Start(&hadc1);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  uint32_t timestamp = 0;
  uint32_t strobeon = 0;
  uint8_t strobeoff = 0;
  while (1)
  {
	  BATTERY_GetVoltage();

	  // HANDLE WIFI CONNECTION
	  Response_t status = WIFI_ReceiveRequest(&wifi, &conn, AT_SHORT_TIMEOUT);
	  if (status == OK)
	  {
		  HAL_GPIO_TogglePin(STATUS_GPIO_Port, STATUS_Pin);
		  char* key_ptr = NULL;

		  if (WIFI_RequestHasKey(&conn, "help"))
		  {
			  SCHEDULE_ReadFromFlash(valve_list, VALVES_NUM);
			  WIFIHANDLER_HandleHelpRequest(&conn);
		  }

		  else if ((key_ptr = WIFI_RequestHasKey(&conn, "valve")))
			  WIFIHANDLER_HandleValveRequest(&conn, valve_list, VALVES_NUM, key_ptr);

		  else if ((key_ptr = WIFI_RequestHasKey(&conn, "wifi")))
			  WIFIHANDLER_HandleWiFiRequest(&conn, key_ptr);

		  else if (conn.request_type == GET)
		  {
			  if ((key_ptr = WIFI_RequestHasKey(&conn, "features")))
				  WIFIHANDLER_HandleFeaturePacket(&conn, valve_list, VALVES_NUM, (char*)FEATURES_TEMPLATE);
			  else if ((key_ptr = WIFI_RequestHasKey(&conn, "notification")))
				  WIFIHANDLER_HandleNotificationRequest(&conn, key_ptr);
			  else if ((key_ptr = WIFI_RequestHasKey(&conn, "weather")))
				  WIFIHANDLER_HandleWeatherRequest(&weather, &conn, key_ptr);
			  else
				  WIFI_SendResponse(&conn, "404 Not Found", "Comando non riconosciuto. Scrivi help per una lista di comandi", 62);
		  }
		  else if (conn.request_type == POST)
		  {
			  if ((key_ptr = WIFI_RequestHasKey(&conn, "at")))
				  AT_ExecuteRemoteATCommand(&conn, key_ptr);
			  else
				  WIFI_SendResponse(&conn, "404 Not Found", "Comando non riconosciuto. Scrivi help per una lista di comandi", 62);
		  }
		  HAL_GPIO_TogglePin(STATUS_GPIO_Port, STATUS_Pin);
	  }
	  else if (status != TIMEOUT)
	  {
		  sprintf(wifi.buf, "Status: %d", status);
		  WIFI_ResetComm(&wifi, &conn);
		  WIFI_SendResponse(&conn, "500 Internal server error", wifi.buf, strlen(wifi.buf));
	  }

	  if (!WIFI_response_sent)
	  {
		  if (status == ERR || status == NULVAL)
			  WIFI_ResetComm(&wifi, &conn);
	  }
	  else
		  WIFI_response_sent = false;

	  if (uwTick - strobeon > STROBE_DELAY)
	  {
		  strobeon = uwTick;
		  strobeoff = 1;
		  HAL_GPIO_TogglePin(STATUS_GPIO_Port, STATUS_Pin);
	  }

	  if (strobeoff && uwTick - strobeon > STROBE_DURATION)
	  {
		  strobeoff = 0;
		  HAL_GPIO_TogglePin(STATUS_GPIO_Port, STATUS_Pin);
	  }

	  // HANDLE WATER FLOW
	  // check flow for each valve
	  for (uint32_t i = 0; i < VALVES_NUM; i++)
	  {
		  Valve_t* valve = &(valve_list[i]);
		  if (valve->flow == NULL) continue;
		  if (uwTick - valve->flow->ic_timestamp > MAX_FLOW_PERIOD)
			  valve->flow->lt_per_hour = 0;
	  }

	  // get time every minute and every 15 minutes get the forecast
	  if (uwTick - timestamp > 60000)
	  {
		  time_hour = WIFI_GetTimeHour(&wifi);
		  time_minute = WIFI_GetTimeMinutes(&wifi);
		  uint8_t seconds = WIFI_GetTimeSeconds(&wifi);
		  timestamp = uwTick - seconds * 1000;

		  // --------- UPDATE FORECAST ---------
		  if (time_minute % 15 == 0)
			  WEATHER_GetForecast(&weather, ESP8266_GetBuffer());

		  if (time_hour == 23)
			  yesterday_last_precipitation_hour = WEATHER_GetTodayLastPrecipitation(&weather);

		  // --------- KEEP VALVES CLOSED IF IT RAINED LESS THAN 12 HOURS AGO ---------
		  bool keep_valves_closed = false;
		  if (yesterday_last_precipitation_hour != -1)
		  {
			  uint8_t time_diff_past = 24 - yesterday_last_precipitation_hour + time_hour;
			  if (time_diff_past <= 12)
				  keep_valves_closed = true;
		  }

		  // --------- KEEP VALVES CLOSED IF IT WILL RAIN IN LESS THAN 12 HOURS ---------
		  uint8_t time_diff_future = WEATHER_GetTodayNextPrecipitation(&weather) - time_hour;
		  if (time_diff_future <= 12)
			  keep_valves_closed = true;

		  if (weather.current_precipitation >= PRECIPITATION_THRESHOLD)
			  keep_valves_closed = true;

		  // --------- CLOSE/OPEN VALVES ---------
		  for (uint32_t i = 0; i < VALVES_NUM; i++)
		  {
			  Valve_t* valve = &(valve_list[i]);

			  if (keep_valves_closed)
			  {
				  NOTIFICATION_Set((char*)NOTIFICATION_WEATHER_NO_VALVE_OPEN, sizeof(NOTIFICATION_WEATHER_NO_VALVE_OPEN));

				  if (!valve->has_manual_override)
					  VALVE_Close(valve);
				  continue;
			  }
			  else if (notification.text == (char*)NOTIFICATION_WEATHER_NO_VALVE_OPEN)
				  NOTIFICATION_Reset();

			  if (time_hour == valve->schedule->hour_close && time_minute == valve->schedule->minute_close)
			  {
				  valve->has_manual_override = false;
				  VALVE_Close(valve);
			  }
			  else if (time_hour == valve->schedule->hour_open && time_minute == valve->schedule->minute_open)
			  {
				  valve->has_manual_override = false;
				  VALVE_Open(valve);
			  }
		  }

		  if (bat.voltage_integer <= 11 && bat.voltage_decimal <= 50)
			  NOTIFICATION_Set((char*)NOTIFICATION_LOW_BATTERY, sizeof(NOTIFICATION_LOW_BATTERY));
		  else if (notification.text == (char*)NOTIFICATION_LOW_BATTERY)
			  NOTIFICATION_Reset();
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
	else if (htim->Instance == TIM1 && htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1)
	{
		// FLOW2
		uint32_t val = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);
		valve_list[1].flow->ic_val = val;
		FLOW_CalculateFlow(valve_list[1].flow);
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

#ifdef  USE_FULL_ASSERT
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
