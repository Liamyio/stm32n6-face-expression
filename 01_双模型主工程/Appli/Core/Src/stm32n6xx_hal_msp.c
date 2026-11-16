/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file         stm32n6xx_hal_msp.c
  * @brief        This file provides code for the MSP Initialization
  *               and de-Initialization codes.
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
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */

/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN Define */

/* USER CODE END Define */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN Macro */

/* USER CODE END Macro */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* External functions --------------------------------------------------------*/
/* USER CODE BEGIN ExternalFunctions */

/* USER CODE END ExternalFunctions */

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */
/**
  * Initializes the Global MSP.
  */
void HAL_MspInit(void)
{

  /* USER CODE BEGIN MspInit 0 */

  /* USER CODE END MspInit 0 */

  /* System interrupt init*/

  /* USER CODE BEGIN MspInit 1 */

  /* USER CODE END MspInit 1 */
}

/* USER CODE BEGIN 1 */

void HAL_I2C_MspInit(I2C_HandleTypeDef *hi2c)
{
  GPIO_InitTypeDef gpio = {0};
  RCC_PeriphCLKInitTypeDef clock = {0};

  if (hi2c->Instance != I2C2)
  {
    return;
  }

  clock.PeriphClockSelection = RCC_PERIPHCLK_I2C2;
  clock.I2c2ClockSelection = RCC_I2C2CLKSOURCE_PCLK1;
  if (HAL_RCCEx_PeriphCLKConfig(&clock) != HAL_OK)
  {
    Error_Handler();
  }

  __HAL_RCC_GPIOD_CLK_ENABLE();
  gpio.Pin = GPIO_PIN_4 | GPIO_PIN_14;
  gpio.Mode = GPIO_MODE_AF_OD;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  gpio.Alternate = GPIO_AF4_I2C2;
  HAL_GPIO_Init(GPIOD, &gpio);

  __HAL_RCC_I2C2_CLK_ENABLE();
}

/* USER CODE END 1 */
