/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
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
#include "memorymap.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "math.h"
#include <stdio.h>
#include <stdbool.h>  //For bool
#include <stdint.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

#define MOTOR_PULSES_PER_REV    2000
#define MAX_MOTOR_FREQ_HZ       30000.0
#define TIMER_CLK_HZ            240000000UL

#define GEAR_RATIO_M1           20
#define GEAR_RATIO_M2           100
#define GEAR_RATIO_M3           40
#define GEAR_RATIO_M4           60

char msg[128];
volatile bool motors_busy = false;
volatile uint32_t psc;
volatile uint32_t arr;
volatile double f_step;
volatile uint32_t ccr;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
double motor_freq[4];
volatile uint32_t motor_pulses[4];
volatile uint32_t motor_pulse_count[4] = { 0, 0, 0, 0 };
double SPEED_INDEX = 1.0;

// Robot parameters
volatile bool dir[4];
volatile int16_t DELTA[4];
volatile bool flag_send_complt;
volatile uint8_t completed_mask = 0;
volatile uint32_t last_debug_time;
// Conversion to motor pulses
double pulses;

double motor_angle[4];
double required_motion[4];

double max_motion = 0.0;
double max_frequency;

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

///////usart1  parameters definition
uint8_t rx_byte;

char motor_packet[27];
uint8_t packet_index = 0;
uint8_t new_motor_packet = 0;

void UART1_ProcessByte(uint8_t c);
void ProcessMotorPacket(void);

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

void UART1_ProcessByte(uint8_t c) {
	/*
	 * We are waiting for the beginning
	 * of a new packet.
	 */
	if (packet_index == 0) {
		if (c == 'A') {
			motor_packet[0] = 'A';
			packet_index = 1;
		}

		return;
	}

	/*
	 * We are already receiving a packet.
	 *
	 * If another A appears, discard the
	 * incomplete packet and start again.
	 */
	if (c == 'A') {
		motor_packet[0] = 'A';
		packet_index = 1;

		return;
	}

	/*
	 * Store the byte.
	 */
	motor_packet[packet_index] = c;
	packet_index++;

	/*
	 * 26 bytes received:
	 *
	 * [0]  A
	 * [1]  DIR1
	 * [2-5] DELTA1
	 *
	 * [6]  DIR2
	 * [7-10] DELTA2
	 *
	 * [11] DIR3
	 * [12-15] DELTA3
	 *
	 * [16] DIR4
	 * [17-20] DELTA4
	 *
	 * [21] B
	 */
	if (packet_index == 22) {
		if (motor_packet[21] == 'B') {
			ProcessMotorPacket();
			new_motor_packet = 1;
		}

		/*
		 * Ready for the next packet.
		 */
		packet_index = 0;
	}
}

void ProcessMotorPacket(void) {

	// Motor 1
	dir[0] = motor_packet[1] - '0';

	DELTA[0] = (motor_packet[2] - '0') * 1000 + (motor_packet[3] - '0') * 100
			+ (motor_packet[4] - '0') * 10 + (motor_packet[5] - '0');

	// Motor 2
	dir[1] = motor_packet[6] - '0';

	DELTA[1] = (motor_packet[7] - '0') * 1000 + (motor_packet[8] - '0') * 100
			+ (motor_packet[9] - '0') * 10 + (motor_packet[10] - '0');

	// Motor 3
	dir[2] = motor_packet[11] - '0';

	DELTA[2] = (motor_packet[12] - '0') * 1000 + (motor_packet[13] - '0') * 100
			+ (motor_packet[14] - '0') * 10 + (motor_packet[15] - '0');

	// Motor 4
	dir[3] = motor_packet[16] - '0';

	DELTA[3] = (motor_packet[17] - '0') * 1000 + (motor_packet[18] - '0') * 100
			+ (motor_packet[19] - '0') * 10 + (motor_packet[20] - '0');

}
void CalculateMotorMotion(const int16_t DELTA[4]) {
	max_motion = 0.0;
	const uint32_t gear[4] = {
	GEAR_RATIO_M1,
	GEAR_RATIO_M2,
	GEAR_RATIO_M3,
	GEAR_RATIO_M4 };
	/*
	 * DELTA is assumed to be in 0.1 degree units.
	 *
	 * Example:
	 * DELTA = 900 means 90.0 degrees.
	 */

	for (uint8_t i = 0; i < 4; i++) {
		motor_angle[i] = fabs(DELTA[i]) / 10.0;

		/*
		 * required_motion is proportional to the
		 * number of motor revolutions required.
		 */
		required_motion[i] = motor_angle[i] * gear[i];

		if (required_motion[i] > max_motion) {
			max_motion = required_motion[i];
		}
	}

	if (max_motion == 0.0) {
		for (uint8_t i = 0; i < 4; i++) {
			motor_freq[i] = 0.0;
			motor_pulses[i] = 0;
		}

		return;
	}

	/*
	 * The motor requiring the largest number of pulses
	 * gets the maximum frequency.
	 */
	max_frequency = MOTOR_PULSES_PER_REV * SPEED_INDEX;

	/*
	 * Calculate exact pulse counts
	 */
	for (uint8_t i = 0; i < 4; i++) {

		pulses = (motor_angle[i] / 360.0) * (double) MOTOR_PULSES_PER_REV
				* (double) gear[i];

		motor_pulses[i] = (uint32_t) llround(pulses);

		/*
		 * Frequency proportional to required pulses
		 */
		motor_freq[i] = max_frequency * required_motion[i] / max_motion;
	}

}

//positive direction --> set dir pin
void SetMotorDirections(bool dir[4]) {
	if (dir[0] == 1)
		HAL_GPIO_WritePin(M1_dir_GPIO_Port,
		M1_dir_Pin, GPIO_PIN_SET);
	else
		HAL_GPIO_WritePin(M1_dir_GPIO_Port,
		M1_dir_Pin, GPIO_PIN_RESET);

	if (dir[1] == 1)
		HAL_GPIO_WritePin(M2_dir_GPIO_Port,
		M2_dir_Pin, GPIO_PIN_SET);
	else
		HAL_GPIO_WritePin(M2_dir_GPIO_Port,
		M2_dir_Pin, GPIO_PIN_RESET);

	if (dir[2] == 1)
		HAL_GPIO_WritePin(M3_dir_GPIO_Port,
		M3_dir_Pin, GPIO_PIN_SET);
	else
		HAL_GPIO_WritePin(M3_dir_GPIO_Port,
		M3_dir_Pin, GPIO_PIN_RESET);

	if (dir[3] == 1)
		HAL_GPIO_WritePin(M4_dir_GPIO_Port,
		M4_dir_Pin, GPIO_PIN_SET);
	else
		HAL_GPIO_WritePin(M4_dir_GPIO_Port,
		M4_dir_Pin, GPIO_PIN_RESET);
}
void StartMotorMove(void) {
	motors_busy = true;
	flag_send_complt = 0;
	completed_mask = 0;

	motor_pulse_count[0] = 0;
	motor_pulse_count[1] = 0;
	motor_pulse_count[2] = 0;
	motor_pulse_count[3] = 0;

	/*
	 * Motors with zero pulses are already complete.
	 */
	if (motor_pulses[0] == 0)
		completed_mask |= (1U << 0);

	if (motor_pulses[1] == 0)
		completed_mask |= (1U << 1);

	if (motor_pulses[2] == 0)
		completed_mask |= (1U << 2);

	if (motor_pulses[3] == 0)
		completed_mask |= (1U << 3);

	/*
	 * Configure frequencies
	 */
	SetMotorFrequency(&htim17,
	TIM_CHANNEL_1, motor_freq[0]);

	SetMotorFrequency(&htim4,
	TIM_CHANNEL_1, motor_freq[1]);

	SetMotorFrequency(&htim3,
	TIM_CHANNEL_2, motor_freq[2]);

	SetMotorFrequency(&htim2,
	TIM_CHANNEL_1, motor_freq[3]);

	/*
	 * Start TIM17
	 */
	if (motor_pulses[0] > 0) {
		__HAL_TIM_SET_COUNTER(&htim17, 0);
		__HAL_TIM_CLEAR_FLAG(&htim17, TIM_FLAG_UPDATE);
		__HAL_TIM_ENABLE_IT(&htim17, TIM_IT_UPDATE);

		HAL_TIM_PWM_Start(&htim17, TIM_CHANNEL_1);
	}

	/*
	 * Start TIM4
	 */
	if (motor_pulses[1] > 0) {
		__HAL_TIM_SET_COUNTER(&htim4, 0);
		__HAL_TIM_CLEAR_FLAG(&htim4, TIM_FLAG_UPDATE);
		__HAL_TIM_ENABLE_IT(&htim4, TIM_IT_UPDATE);

		HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1);
	}

	/*
	 * Start TIM3
	 */
	if (motor_pulses[2] > 0) {
		__HAL_TIM_SET_COUNTER(&htim3, 0);
		__HAL_TIM_CLEAR_FLAG(&htim3, TIM_FLAG_UPDATE);
		__HAL_TIM_ENABLE_IT(&htim3, TIM_IT_UPDATE);

		HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
	}

	/*
	 * Start TIM2
	 */
	if (motor_pulses[3] > 0) {
		__HAL_TIM_SET_COUNTER(&htim2, 0);
		__HAL_TIM_CLEAR_FLAG(&htim2, TIM_FLAG_UPDATE);
		__HAL_TIM_ENABLE_IT(&htim2, TIM_IT_UPDATE);

		HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
	}

}
void SetMotorFrequency(TIM_HandleTypeDef *htim, uint32_t Channel,
		double frequency) {
	uint32_t psc;
	uint32_t arr;
	uint32_t ccr;

	if (frequency <= 0.0) {
		HAL_TIM_PWM_Stop(htim, Channel);
		__HAL_TIM_DISABLE_IT(htim, TIM_IT_UPDATE);
		return;
	}

	/*
	 * Find the smallest prescaler that allows ARR
	 * to fit into a 16-bit timer.
	 *
	 * TIM2 is 32-bit, but this also works for it.
	 */
	psc = 0;

	while ((TIMER_CLK_HZ / ((uint64_t) (psc + 1) * frequency)) > 65536.0) {
		psc++;

		if (psc >= 65535)
			break;
	}

	double period_counts =
	TIMER_CLK_HZ / ((double) (psc + 1) * frequency);

	arr = (uint32_t) llround(period_counts - 1.0);

	if (arr > 65535)
		arr = 65535;

	ccr = (arr + 1) / 2;

	HAL_TIM_PWM_Stop(htim, Channel);
	__HAL_TIM_DISABLE_IT(htim, TIM_IT_UPDATE);

	__HAL_TIM_SET_PRESCALER(htim, psc);
	__HAL_TIM_SET_AUTORELOAD(htim, arr);
	__HAL_TIM_SET_COMPARE(htim, Channel, ccr);
	__HAL_TIM_SET_COUNTER(htim, 0);

	HAL_TIM_GenerateEvent(htim, TIM_EVENTSOURCE_UPDATE);
	__HAL_TIM_CLEAR_FLAG(htim, TIM_FLAG_UPDATE);
}
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
	if (htim->Instance == TIM17) {
		motor_pulse_count[0]++;

		if (motor_pulse_count[0] >= motor_pulses[0]) {
			HAL_TIM_PWM_Stop(&htim17, TIM_CHANNEL_1);
			__HAL_TIM_DISABLE_IT(&htim17, TIM_IT_UPDATE);

            motor_pulse_count[0] = 0;
			completed_mask |= (1U << 0);
		}
	}

	else if (htim->Instance == TIM4) {
		motor_pulse_count[1]++;

		if (motor_pulse_count[1] >= motor_pulses[1]) {
			HAL_TIM_PWM_Stop(&htim4, TIM_CHANNEL_1);
			__HAL_TIM_DISABLE_IT(&htim4, TIM_IT_UPDATE);

            motor_pulse_count[1] = 0;
			completed_mask |= (1U << 1);
		}
	}

	else if (htim->Instance == TIM3) {
		motor_pulse_count[2]++;

		if (motor_pulse_count[2] >= motor_pulses[2]) {
			HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_2);
			__HAL_TIM_DISABLE_IT(&htim3, TIM_IT_UPDATE);

            motor_pulse_count[2] = 0;
			completed_mask |= (1U << 2);
		}
	}

	else if (htim->Instance == TIM2) {
		motor_pulse_count[3]++;

		if (motor_pulse_count[3] >= motor_pulses[3]) {
			HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_1);
			__HAL_TIM_DISABLE_IT(&htim2, TIM_IT_UPDATE);

            motor_pulse_count[3] = 0;
			completed_mask |= (1U << 3);
		}
	}

	if (completed_mask == 0x0F) {
		flag_send_complt = 1;
		motors_busy = false;
	}
}

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {

	/* USER CODE BEGIN 1 */

	/* USER CODE END 1 */

	/* MPU Configuration--------------------------------------------------------*/
	MPU_Config();

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
	MX_TIM3_Init();
	MX_TIM4_Init();
	MX_TIM17_Init();
	MX_UART7_Init();
	MX_USART1_UART_Init();
	MX_TIM2_Init();
	/* USER CODE BEGIN 2 */

	/* USER CODE END 2 */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	while (1) {
		if (HAL_UART_Receive(&huart1, &rx_byte, 1, 1) == HAL_OK)
			UART1_ProcessByte(rx_byte);



		if (new_motor_packet && !motors_busy) {
			new_motor_packet = 0;
			CalculateMotorMotion(DELTA);

			sprintf(msg, "%4u,%4u,%4u,%4u \r\n", motor_pulses[0], motor_pulses[1],
					motor_pulses[2], motor_pulses[3]);
			HAL_UART_Transmit(&huart7, (uint8_t*) msg, strlen(msg), 10);

			sprintf(msg, "%8.2f,%8.2f,%8.2f,%8.2f\r\n", motor_freq[0],
					motor_freq[1], motor_freq[2], motor_freq[3]);
			HAL_UART_Transmit(&huart7, (uint8_t*) msg, strlen(msg), 10);
			if (max_motion > 0.0) {
				SetMotorDirections(dir);
				StartMotorMove();
			}
		}
//		if (flag_send_complt)
//		{
//		    flag_send_complt = 0;
//
//		    sprintf(msg, "DONE: %lu,%lu,%lu,%lu\r\n",
//		            (unsigned long)motor_pulse_count[0],
//		            (unsigned long)motor_pulse_count[1],
//		            (unsigned long)motor_pulse_count[2],
//		            (unsigned long)motor_pulse_count[3]);
//
//		    HAL_UART_Transmit(&huart7, (uint8_t *)msg, strlen(msg), 10);
//		}
//		if (HAL_GetTick() - last_debug_time >= 100)
//		{
//		    last_debug_time = HAL_GetTick();
//
//		    sprintf(msg, "%lu,%lu,%lu,%lu\r\n",
//		            (unsigned long)motor_pulse_count[0],
//		            (unsigned long)motor_pulse_count[1],
//		            (unsigned long)motor_pulse_count[2],
//		            (unsigned long)motor_pulse_count[3]);
//
//		    HAL_UART_Transmit(&huart7, (uint8_t*)msg, strlen(msg), 10);
//		}
		/* USER CODE END WHILE */

		/* USER CODE BEGIN 3 */
	}
	/* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
	RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
	RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };

	/** Supply configuration update enable
	 */
	HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

	/** Configure the main internal regulator output voltage
	 */
	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

	while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {
	}

	/** Initializes the RCC Oscillators according to the specified parameters
	 * in the RCC_OscInitTypeDef structure.
	 */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
	RCC_OscInitStruct.HSEState = RCC_HSE_ON;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
	RCC_OscInitStruct.PLL.PLLM = 1;
	RCC_OscInitStruct.PLL.PLLN = 120;
	RCC_OscInitStruct.PLL.PLLP = 2;
	RCC_OscInitStruct.PLL.PLLQ = 2;
	RCC_OscInitStruct.PLL.PLLR = 2;
	RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
	RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
	RCC_OscInitStruct.PLL.PLLFRACN = 0;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
		Error_Handler();
	}

	/** Initializes the CPU, AHB and APB buses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
			| RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 | RCC_CLOCKTYPE_D3PCLK1
			| RCC_CLOCKTYPE_D1PCLK1;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
	RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
	RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK) {
		Error_Handler();
	}
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/* MPU Configuration */

void MPU_Config(void) {
	MPU_Region_InitTypeDef MPU_InitStruct = { 0 };

	/* Disables the MPU */
	HAL_MPU_Disable();

	/** Initializes and configures the Region and the memory to be protected
	 */
	MPU_InitStruct.Enable = MPU_REGION_ENABLE;
	MPU_InitStruct.Number = MPU_REGION_NUMBER0;
	MPU_InitStruct.BaseAddress = 0x0;
	MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
	MPU_InitStruct.SubRegionDisable = 0x87;
	MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
	MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
	MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
	MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
	MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
	MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

	HAL_MPU_ConfigRegion(&MPU_InitStruct);
	/* Enables the MPU */
	HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
	/* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	__disable_irq();
	while (1) {
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
