/* USER CODE BEGIN Header */
/**
  * Laboratory Activity 5: RTOS on STM32 (RT-Spark)
  * FINAL FIX: HYBRID TIMER SOLUTION
  * ------------------------------------------------
  * Problem: PA7 causes Matrix interference.
  * Solution: Avoid PA7. Use PA6 (TIM3) and PE11/PE13 (TIM1).
  *
  * Wiring:
  * - Red:   PA6  (TIM3_CH1)
  * - Green: PE11 (TIM1_CH2)
  * - Blue:  PE13 (TIM1_CH3)
  */
/* USER CODE END Header */

#include "main.h"
#include "cmsis_os.h"
#include "drv_lcd.h"
#include "drv_aht21.h"
#include <stdlib.h>
#include <math.h>

/* Private variables */
ADC_HandleTypeDef hadc1;
SRAM_HandleTypeDef hsram1;
TIM_HandleTypeDef htim1; // Controls Green/Blue
TIM_HandleTypeDef htim3; // Controls Red

/* RTOS Handles */
osThreadId_t TempTaskHandle;
osThreadId_t RGBTaskHandle;
osThreadId_t CounterTaskHandle;
osThreadId_t BlinkTaskHandle;
osMutexId_t  lcdMutexHandle;

/* Prototypes */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM1_Init(void);
static void MX_FSMC_Init(void);

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);
void Kill_Onboard_Hardware(void);

void StartTempTask(void *argument);
void StartRGBTask(void *argument);
void StartCounterTask(void *argument);
void StartBlinkTask(void *argument);

/* --- HELPER FUNCTIONS --- */
void LCD_DrawRect(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color) {
    LCD_DrawPixel(x1, y1, color); LCD_DrawPixel(x2, y2, color);
    for (uint16_t x = x1; x <= x2; x++) { LCD_DrawPixel(x, y1, color); LCD_DrawPixel(x, y2, color); }
    for (uint16_t y = y1; y <= y2; y++) { LCD_DrawPixel(x1, y, color); LCD_DrawPixel(x2, y, color); }
}
void LCD_ShowNum(uint16_t x, uint16_t y, int num, uint16_t color, uint16_t back_color) {
    char buf[12]; int i = 0;
    if (num == 0) { LCD_ShowChar(x, y, '0', color, back_color); return; }
    if (num < 0) { LCD_ShowChar(x, y, '-', color, back_color); x += 8; num = -num; }
    while (num > 0) { buf[i++] = (num % 10) + '0'; num /= 10; }
    while (--i >= 0) { LCD_ShowChar(x, y, buf[i], color, back_color); x += 8; }
}
void LCD_ShowFloatManual(uint16_t x, uint16_t y, float val, uint16_t color, uint16_t back_color) {
    int intPart = (int)val;
    LCD_ShowNum(x, y, intPart, color, back_color);
    int offset = 8 + (intPart<0?8:0) + (abs(intPart)>9?8:0) + (abs(intPart)>99?8:0);
    LCD_ShowChar(x + offset, y, '.', color, back_color);
    int decPart = (int)((val - (float)intPart) * 100.0f);
    if (decPart < 0) decPart = -decPart;
    if (decPart < 10) { LCD_ShowChar(x+offset+8, y, '0', color, back_color); LCD_ShowNum(x+offset+16, y, decPart, color, back_color); }
    else { LCD_ShowNum(x+offset+8, y, decPart, color, back_color); }
}
#define FILTER_SIZE 10
float Filter_Value(float new_val, float *buffer, uint8_t *index, uint8_t *count) {
    if (new_val > 100.0f || new_val < -40.0f) {
        if (*count > 0) { float sum=0; for(int i=0; i<*count; i++) sum+=buffer[i]; return sum / *count; }
        return new_val;
    }
    buffer[*index] = new_val;
    *index = (*index + 1) % FILTER_SIZE;
    if (*count < FILTER_SIZE) (*count)++;
    float sum = 0; for (uint8_t i = 0; i < *count; i++) sum += buffer[i];
    return sum / (float)(*count);
}

/* --- MAIN FUNCTION --- */
int main(void)
{
  HAL_Init();
  SystemClock_Config();

  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_TIM3_Init(); // Red (PA6)
  MX_TIM1_Init(); // Green (PE11), Blue (PE13)
  MX_FSMC_Init();

  /* 1. KILL ONBOARD INTERFERENCE */
  Kill_Onboard_Hardware();

  /* 2. START PWM CHANNELS */
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1); // Red
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2); // Green
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3); // Blue

  /* 3. DRIVER INIT */
  LCD_Init();
  LCD_Clear(WHITE);
  LCD_DrawRect(5, 5, 235, 40, BLUE);
  LCD_ShowString(40, 15, "RT-Spark FreeRTOS", BLUE, WHITE);
  LCD_ShowString(20, 60, "Temp:", BLACK, WHITE);
  LCD_ShowString(20, 80, "Hum: ", BLACK, WHITE);
  LCD_ShowString(20, 120, "Counter:", BLACK, WHITE);
  LCD_ShowString(20, 160, "LED Bright:", BLACK, WHITE);

  AHT21_Init();

  /* 4. RTOS INIT */
  osKernelInitialize();

  const osMutexAttr_t lcdMutex_attr = { .name = "LCDMutex" };
  lcdMutexHandle = osMutexNew(&lcdMutex_attr);

  const osThreadAttr_t tempTask_attr = { .name = "TempTask", .stack_size = 1024, .priority = (osPriority_t) osPriorityNormal };
  TempTaskHandle = osThreadNew(StartTempTask, NULL, &tempTask_attr);

  const osThreadAttr_t rgbTask_attr = { .name = "RGBTask", .stack_size = 512, .priority = (osPriority_t) osPriorityNormal };
  RGBTaskHandle = osThreadNew(StartRGBTask, NULL, &rgbTask_attr);

  const osThreadAttr_t countTask_attr = { .name = "CountTask", .stack_size = 256, .priority = (osPriority_t) osPriorityNormal };
  CounterTaskHandle = osThreadNew(StartCounterTask, NULL, &countTask_attr);

  const osThreadAttr_t blinkTask_attr = { .name = "BlinkTask", .stack_size = 128, .priority = (osPriority_t) osPriorityLow };
  BlinkTaskHandle = osThreadNew(StartBlinkTask, NULL, &blinkTask_attr);

  osKernelStart();
  while (1) { __WFI(); }
}

void Kill_Onboard_Hardware(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // 1. Force PF2 (Matrix Power) LOW
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_2, GPIO_PIN_RESET);
    GPIO_InitStruct.Pin = GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

    // 2. Force PF12 (Red LED) HIGH (OFF)
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_12, GPIO_PIN_SET);
    GPIO_InitStruct.Pin = GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

    // 3. Force PA7 (Matrix Data) to Input/PullDown (Safety)
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitStruct.Pin = GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

void StartRGBTask(void *argument) {
    uint32_t adc_val;
    uint32_t last_adc_val = 9999;
    int brightness_pct;
    uint32_t pwm_val;

    for(;;) {
        HAL_ADC_Start(&hadc1);
        if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) {
            adc_val = HAL_ADC_GetValue(&hadc1);
        }

        // Deadzone
        if (adc_val < 150) adc_val = 0;

        // Hysteresis
        if (abs((int)adc_val - (int)last_adc_val) > 20) {
            last_adc_val = adc_val;

            pwm_val = (adc_val * 999) / 4095;
            brightness_pct = (adc_val * 100) / 4095;

            // Update Colors (Mixed Timers)
            __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, pwm_val); // Red (PA6)
            __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, pwm_val); // Green (PE11)
            __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, pwm_val); // Blue (PE13)

            if (osMutexAcquire(lcdMutexHandle, 100) == osOK) {
                 LCD_ShowString(120, 160, "    ", WHITE, WHITE);
                 LCD_ShowNum(120, 160, brightness_pct, BLACK, WHITE);
                 LCD_ShowChar(150, 160, '%', BLACK, WHITE);
                 osMutexRelease(lcdMutexHandle);
            }
        }
        osDelay(50);
    }
}

void StartBlinkTask(void *argument) {
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_12, GPIO_PIN_SET);
    for(;;) {
        HAL_GPIO_TogglePin(GPIOF, GPIO_PIN_11);
        HAL_GPIO_WritePin(GPIOF, GPIO_PIN_12, GPIO_PIN_SET);
        osDelay(200);
    }
}

// ... (StartTempTask, StartCounterTask) ...
void StartTempTask(void *argument) {
    float raw_temp=0, raw_hum=0, final_temp=0, final_hum=0;
    float temp_buf[FILTER_SIZE]={0}, hum_buf[FILTER_SIZE]={0};
    uint8_t t_idx=0, h_idx=0, t_cnt=0, h_cnt=0;
    for(;;) {
        if (AHT21_Read(&raw_temp, &raw_hum) == 1) {
            final_temp = Filter_Value(raw_temp, temp_buf, &t_idx, &t_cnt);
            final_hum  = Filter_Value(raw_hum, hum_buf, &h_idx, &h_cnt);
            if (osMutexAcquire(lcdMutexHandle, 500) == osOK) {
                LCD_ShowString(70, 60, "      ", WHITE, WHITE);
                LCD_ShowFloatManual(70, 60, final_temp, RED, WHITE);
                LCD_ShowChar(130, 60, 'C', RED, WHITE);
                LCD_ShowString(70, 80, "      ", WHITE, WHITE);
                LCD_ShowFloatManual(70, 80, final_hum, BLUE, WHITE);
                LCD_ShowChar(130, 80, '%', BLUE, WHITE);
                osMutexRelease(lcdMutexHandle);
            }
        }
        osDelay(1000);
    }
}
void StartCounterTask(void *argument) {
    int counter = 0;
    for(;;) {
        counter++;
        if (counter > 100) counter = 0;
        if (osMutexAcquire(lcdMutexHandle, 500) == osOK) {
            LCD_ShowString(100, 120, "     ", WHITE, WHITE);
            LCD_ShowNum(100, 120, counter, BLACK, WHITE);
            osMutexRelease(lcdMutexHandle);
        }
        osDelay(500);
    }
}

/* --- PERIPHERAL INIT --- */
static void MX_TIM1_Init(void) {
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 15;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 999;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  HAL_TIM_PWM_Init(&htim1);
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig);
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  // Configure PE11 (CH2) and PE13 (CH3)
  HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_2);
  HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_3);
  HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig);
  HAL_TIM_MspPostInit(&htim1);
}

static void MX_TIM3_Init(void) {
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 15;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 999;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  HAL_TIM_PWM_Init(&htim3);
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig);
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  // Configure PA6 (CH1)
  HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1);
  HAL_TIM_MspPostInit(&htim3);
}

/*void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim) {
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if (htim->Instance == TIM3) {
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitStruct.Pin = GPIO_PIN_6; // PA6 ONLY (No PA7)
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF2_TIM3;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
  }
  else if (htim->Instance == TIM1) {
    __HAL_RCC_GPIOE_CLK_ENABLE();
    GPIO_InitStruct.Pin = GPIO_PIN_11|GPIO_PIN_13; // PE11 and PE13
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF1_TIM1;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
  }
}*/

static void MX_GPIO_Init(void) {
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  __HAL_RCC_GPIOF_CLK_ENABLE(); __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE(); __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  HAL_GPIO_WritePin(GPIOF, GPIO_PIN_11|GPIO_PIN_12, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOF, GPIO_PIN_9|GPIO_PIN_2|GPIO_PIN_13, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin = GPIO_PIN_11|GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_9|GPIO_PIN_2|GPIO_PIN_13;
  HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);
  HAL_GPIO_WritePin(GPIOF, GPIO_PIN_13, GPIO_PIN_SET);

  GPIO_InitStruct.Pin = GPIO_PIN_3;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_3, GPIO_PIN_SET);

  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
  HAL_GPIO_WritePin(GPIOE, GPIO_PIN_0|GPIO_PIN_1, GPIO_PIN_SET);
}

static void MX_ADC1_Init(void) {
  ADC_ChannelConfTypeDef sConfig = {0};
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  HAL_ADC_Init(&hadc1);
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
  HAL_ADC_ConfigChannel(&hadc1, &sConfig);
}

static void MX_FSMC_Init(void) {
  FSMC_NORSRAM_TimingTypeDef Timing = {0};
  hsram1.Instance = FSMC_NORSRAM_DEVICE;
  hsram1.Extended = FSMC_NORSRAM_EXTENDED_DEVICE;
  hsram1.Init.NSBank = FSMC_NORSRAM_BANK3;
  hsram1.Init.DataAddressMux = FSMC_DATA_ADDRESS_MUX_DISABLE;
  hsram1.Init.MemoryType = FSMC_MEMORY_TYPE_SRAM;
  hsram1.Init.MemoryDataWidth = FSMC_NORSRAM_MEM_BUS_WIDTH_8;
  hsram1.Init.WriteOperation = FSMC_WRITE_OPERATION_ENABLE;
  hsram1.Init.ExtendedMode = FSMC_EXTENDED_MODE_DISABLE;
  hsram1.Init.AsynchronousWait = FSMC_ASYNCHRONOUS_WAIT_DISABLE;
  hsram1.Init.WriteBurst = FSMC_WRITE_BURST_DISABLE;
  hsram1.Init.PageSize = FSMC_PAGE_SIZE_NONE;
  Timing.AddressSetupTime = 15;
  Timing.AddressHoldTime = 15;
  Timing.DataSetupTime = 60;
  Timing.BusTurnAroundDuration = 5;
  Timing.CLKDivision = 16;
  Timing.DataLatency = 17;
  Timing.AccessMode = FSMC_ACCESS_MODE_A;
  HAL_SRAM_Init(&hsram1, &Timing, NULL);
}

void SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
  if (htim->Instance == TIM6) HAL_IncTick();
}

void Error_Handler(void) { __disable_irq(); while (1) {} }
