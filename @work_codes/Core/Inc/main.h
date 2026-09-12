/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32h7xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */
void Set_Motor_RPM(TIM_HandleTypeDef *htim, uint32_t Channel,uint16_t rpm);
void UART1_ProcessByte(uint8_t c);
void ProcessMotorPacket(void);
/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define M4_Tim2Ch1_Pin GPIO_PIN_15
#define M4_Tim2Ch1_GPIO_Port GPIOA
#define M4_dir_Pin GPIO_PIN_10
#define M4_dir_GPIO_Port GPIOC
#define M3_dir_Pin GPIO_PIN_2
#define M3_dir_GPIO_Port GPIOD
#define M3_Tim3Ch2_Pin GPIO_PIN_5
#define M3_Tim3Ch2_GPIO_Port GPIOB
#define M2_Tim4Ch1_Pin GPIO_PIN_6
#define M2_Tim4Ch1_GPIO_Port GPIOB
#define M2_dir_Pin GPIO_PIN_7
#define M2_dir_GPIO_Port GPIOB
#define M1_Tim17Ch1_Pin GPIO_PIN_9
#define M1_Tim17Ch1_GPIO_Port GPIOB
#define M1_dir_Pin GPIO_PIN_0
#define M1_dir_GPIO_Port GPIOE

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
