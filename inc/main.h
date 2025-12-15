/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define GPIO_LCD_BL_Pin GPIO_PIN_9
#define GPIO_LCD_BL_GPIO_Port GPIOF
#define LED_Y_Pin GPIO_PIN_11
#define LED_Y_GPIO_Port GPIOF
#define GPIO_LCD_RST_Pin GPIO_PIN_3
#define GPIO_LCD_RST_GPIO_Port GPIOD
#define AHT_SCL_Pin GPIO_PIN_0
#define AHT_SCL_GPIO_Port GPIOE
#define AHT_SDA_Pin GPIO_PIN_1
#define AHT_SDA_GPIO_Port GPIOE

/* USER CODE BEGIN Private defines */

// --- LCD PINS ---
// Chip Select (CS) -> PF13
#define LCD_CS_Pin      GPIO_PIN_13
#define LCD_CS_GPIO_Port GPIOF

// Backlight (BL) -> PF9 (From your previous code)
#define LCD_BL_Pin      GPIO_PIN_9
#define LCD_BL_GPIO_Port GPIOF

// Reset (RST) -> PD3 (From your previous code)
#define LCD_RST_Pin     GPIO_PIN_3
#define LCD_RST_GPIO_Port GPIOD

// --- RGB MATRIX PINS ---
// Power Enable -> PF2
#define RGB_EN_Pin      GPIO_PIN_2
#define RGB_EN_GPIO_Port GPIOF

// --- AHT21 PINS ---
// Software I2C
#define AHT_SCL_Pin     GPIO_PIN_0
#define AHT_SCL_GPIO_Port GPIOE
#define AHT_SDA_Pin     GPIO_PIN_1
#define AHT_SDA_GPIO_Port GPIOE

// --- OTHER PINS ---
#define LED_BLINK_Pin   GPIO_PIN_11 // Yellow/Blue LED
#define LED_BLINK_GPIO_Port GPIOF

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
