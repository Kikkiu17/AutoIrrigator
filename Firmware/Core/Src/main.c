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
#include "dma.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdio.h>

#include "..\ESP8266\esp8266.h"
#include "../wifihandler/wifihandler.h"
#include "../credentials.h"
#include "../irrigator/irrigator.h"
#include "../weather/weather.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define NUM_VALVES 4
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
const uint32_t MAX_FLOW_PERIOD = 1 / ((float)MIN_WATER_FLOW / 60.0 * 7.5) * 1000;

uint8_t time_hour;
uint8_t time_minute;

WIFI_t wifi;
Connection_t conn;

Valve_t valve_list[NUM_VALVES];
Flow_t flow1;
Flow_t flow2;
Flow_t flow3;
Flow_t flow4;
Schedule_t schedule1;
Schedule_t schedule2;
Schedule_t schedule3;
Schedule_t schedule4;

Weather_t weather;

const char ESP_NAME[] = "Hub irrigazione";
//const char ESP_HOSTNAME[] = "ESPDEVICE002"; // template: ESPDEVICExxx
//const char ESP_IP[] = "192.168.1.38";
const char SERVER_PORT[] = "34677";

/**
 * template:
 * type1$Name:data;
 * type2$Name,additional_feature$feature_Name$data,additional_feature$feature_name$data...;
 *
 * every type must have a numerical ID (typeX - X being the ID).
 * every type must have a name.
 * a type can have additional features, that must be put on the same line of the main feature,
 * preceded by a comma ",".
 * a semicolon ";" must be put at the end of each feature (line).
 *
 * example:
 * switch1$Switch number one,sensor$Switch status$%d;
 * switch2$Switch number two,sensor$Switch status$%d,sensor$Time$%d;
 * timestamp1$Uptime$%d;
 * sensor1$Battery voltage$%d;
 */
const char FEATURES_TEMPLATE[] =
{
		"switch1$Valvola 1,status$%d,sensor$Litri/h$%d;"
		"switch2$Valvola 2,status$%d,sensor$Litri/h$%d;"
		"switch3$Valvola 3,status$%d,sensor$Litri/h$%d;"
		"switch4$Valvola 4,status$%d,sensor$Litri/h$%d;"
		//"textinput1$%s,button$Imposta$textInputPOST ?valve=1&schedule=;"
		//"textinput2$%s,button$Imposta$textInputPOST ?valve=2&schedule=;"
		//"textinput3$%s,button$Imposta$textInputPOST ?valve=3&schedule=;"
		//"textinput4$%s,button$Imposta$textInputPOST ?valve=4&schedule=;"
		"timepicker1$%s,button$Imposta$sendPOST ?valve=1&schedule=;"
		"timepicker2$%s,button$Imposta$sendPOST ?valve=2&schedule=;"
		"timepicker3$%s,button$Imposta$sendPOST ?valve=3&schedule=;"
		"timepicker4$%s,button$Imposta$sendPOST ?valve=4&schedule=;"
		//"textinput1$NOME 1,button$Imposta$sendPOST ?valve=1&cmd=;"
		"timestamp1$Tempo CPU$%d ms;"
};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

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
  /* USER CODE BEGIN 2 */
  // ESPRST is HIGH by default (set up in .ioc file) so ESP is enabled by default
  ESP8266_Init();

  if (ESP8266_CheckAT() != OK)
  {
	  // reset ESP
	  Response_t start_ok = ERR;
	  while (start_ok != OK)
	  {
		  if (ESP8266_ATReset() != OK)
		  {
			  // hardware reset
			  HAL_GPIO_WritePin(ESPRST_GPIO_Port, ESPRST_Pin, 0);
			  HAL_Delay(1);
			  HAL_GPIO_WritePin(ESPRST_GPIO_Port, ESPRST_Pin, 1);
		  }
		  HAL_GPIO_TogglePin(STATUS_GPIO_Port, STATUS_Pin);
		  start_ok = ESP8266_WaitForStringCNDTROffset("ready", -10, 5000);
		  HAL_GPIO_TogglePin(STATUS_GPIO_Port, STATUS_Pin);
		  __HAL_UART_CLEAR_OREFLAG(&huart1);	// clear overrun flag caused by esp reset
		  ESP8266_ClearBuffer();
	  }
  }

  HAL_GPIO_TogglePin(STATUS_GPIO_Port, STATUS_Pin);
  // wait for WiFi, otherwise it will timeout and connect to the WiFi
  if (ESP8266_WaitForStringCNDTROffset("WIFI CONNECTED", -20, 6000) == OK)
	  ESP8266_WaitForStringCNDTROffset("WIFI GOT IP", -15, 18000);
  HAL_GPIO_TogglePin(STATUS_GPIO_Port, STATUS_Pin);

  memcpy(wifi.SSID, ssid, strlen(ssid));
  memcpy(wifi.pw, password, strlen(password));

  WIFI_Connect(&wifi);
  //WIFI_SetIP(&wifi, (char*)ESP_IP);
  //WIFI_SetHostname(&wifi, (char*)ESP_HOSTNAME);
  WIFI_SetName(&wifi, (char*)ESP_NAME);
  WIFI_EnableNTPServer(&wifi, 2);

  HAL_GPIO_TogglePin(STATUS_GPIO_Port, STATUS_Pin);
  Response_t atstatus = ESP8266_CheckAT();
  /**
   * if some of the following commands return something other than OK, it could mean that
   * the server is already set up, so we can continue the boot
   */
  if (atstatus == OK)
	  atstatus = WIFI_SetCWMODE("1");
  if (atstatus == OK)
  	  atstatus = WIFI_SetCIPMUX("1");
  if (atstatus == OK)
  	  atstatus = WIFI_SetCIPSERVER((char*)SERVER_PORT);
  HAL_GPIO_TogglePin(STATUS_GPIO_Port, STATUS_Pin);

  VALVE_Init(&valve_list[0], &flow1, &schedule1, 1, VALVE1_GPIO_Port, VALVE1_Pin);
  VALVE_Init(&valve_list[1], &flow2, &schedule2, 2, VALVE2_GPIO_Port, VALVE2_Pin);
  VALVE_Init(&valve_list[2], &flow3, &schedule3, 3, VALVE3_GPIO_Port, VALVE3_Pin);
  VALVE_Init(&valve_list[3], &flow4, &schedule4, 4, VALVE4_GPIO_Port, VALVE4_Pin);

  HAL_TIM_IC_Start_IT(&htim14, TIM_CHANNEL_1);
  HAL_TIM_IC_Start_IT(&htim1, TIM_CHANNEL_1);
  HAL_TIM_IC_Start_IT(&htim3, TIM_CHANNEL_1);
  HAL_TIM_IC_Start_IT(&htim3, TIM_CHANNEL_2);

  WEATHER_GetForecast(&weather, ESP8266_GetBuffer());
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  uint32_t timestamp = 0;
  while (1)
  {
	  // HANDLE WIFI CONNECTION
	  atstatus = WIFI_ReceiveRequest(&wifi, &conn, AT_SHORT_TIMEOUT);
	  if (atstatus == OK)
	  {
		  HAL_GPIO_TogglePin(STATUS_GPIO_Port, STATUS_Pin);
		  char* key_ptr = NULL;

		  if (WIFI_RequestHasKey(&conn, "help"))
			  WIFIHANDLER_HandleHelpRequest(&conn);

		  else if ((key_ptr = WIFI_RequestHasKey(&conn, "valve")))
			  WIFIHANDLER_HandleValveRequest(&conn, valve_list, NUM_VALVES, key_ptr);

		  else if ((key_ptr = WIFI_RequestHasKey(&conn, "wifi")))
			  WIFIHANDLER_HandleWiFiRequest(&conn, key_ptr);

		  else if (conn.request_type == GET)
		  {
			  if ((key_ptr = WIFI_RequestHasKey(&conn, "features")))
				  WIFIHANDLER_HandleFeaturePacket(&conn, valve_list, NUM_VALVES, (char*)FEATURES_TEMPLATE);
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
	  else if (atstatus != TIMEOUT)
	  {
		  sprintf(wifi.buf, "Status: %d", atstatus);
		  WIFI_SendResponse(&conn, "500 Internal server error", wifi.buf, strlen(wifi.buf));
	  }
	  // HANDLE WATER FLOW
	  // check flow for each valve
	  for (uint32_t i = 0; i < NUM_VALVES; i++)
	  {
		  Valve_t* valve = &(valve_list[i]);
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

		  if (time_minute % 15 == 0)
			  WEATHER_GetForecast(&weather, ESP8266_GetBuffer());

		  uint8_t will_rain = 0;
		  for (uint32_t i = 0; i < 24; i++)
		  {
			  if (weather.hourly_precipitation[i] / 1000 > 1)	// more than 1 mm
			  {
				  will_rain = 1;
				  break;
			  }
		  }

		  for (uint32_t i = 0; i < NUM_VALVES; i++)
		  {
			  Valve_t* valve = &(valve_list[i]);

			  if (will_rain)
			  {
				  VALVE_Close(valve);
				  continue;
			  }

			  if (time_hour == valve->schedule->hour_open && time_minute == valve->schedule->minute_open)
				  VALVE_Open(valve);
			  else if (time_hour == valve->schedule->hour_close && time_minute == valve->schedule->minute_close)
				  VALVE_Close(valve);
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
