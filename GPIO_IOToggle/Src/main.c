/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    GPIO/GPIO_IOToggle/Src/main.c
  * @author  MCD Application Team
  * @brief   This example describes how to configure and use GPIOs through
  *          the STM32U5xx HAL API.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2022 STMicroelectronics.
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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include "lvgl.h"
#include "lvgl_port.h"
#include "ui_pages.h"
#include "rtc_clock.h"
#include "stm32u5x9j_discovery_env_sensors.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
static volatile int in_stop;
static volatile int sleep_request;
static GPIO_InitTypeDef  GPIO_InitStruct;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void SystemPower_Config(void);
static void MX_ICACHE_Init(void);
/* USER CODE BEGIN PFP */
static void Temp_Init(void);
static int  Temp_Read(int32_t *tenths);
static void Uart_Rx_Start(void);
static void Uart_Poll_Command(void);
static void Power_Sleep(void);

/* ALWAYS_ON = 1: the watch face never turns off by itself (no idle timeout).
 * The USER button still puts the board to sleep / wakes it manually.
 * ALWAYS_ON = 0: display off + Stop 2 after SLEEP_AFTER_MS of no touch. */
#define ALWAYS_ON        1
#define SLEEP_AFTER_MS   60000U

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
  /* STM32U5xx HAL library initialization:
       - Configure the Flash prefetch
       - Configure the Systick to generate an interrupt each 1 msec
       - Set NVIC Group Priority to 3
       - Low Level Initialization
     */
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* Configure the System Power */
  SystemPower_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_ICACHE_Init();
  /* USER CODE BEGIN 2 */

   /* -1- Enable GPIO Clock (to be able to program the configuration registers) */
  LED3_GPIO_CLK_ENABLE();
  LED4_GPIO_CLK_ENABLE();

  /* -2- Configure IO in output push-pull mode to drive external LEDs */
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;

  GPIO_InitStruct.Pin = LED3_PIN;
  HAL_GPIO_Init(LED3_GPIO_PORT, &GPIO_InitStruct);
  GPIO_InitStruct.Pin = LED4_PIN;
  HAL_GPIO_Init(LED4_GPIO_PORT, &GPIO_InitStruct);

  /* -3- UART: COM1 = USART1 (PA9/PA10) -> ST-LINK Virtual COM port, 115200 8N1 */
  COM_InitTypeDef com = {
    .BaudRate   = 115200,
    .WordLength = COM_WORDLENGTH_8B,
    .StopBits   = COM_STOPBITS_1,
    .Parity     = COM_PARITY_NONE,
    .HwFlowCtl  = COM_HWCONTROL_NONE,
  };
  if (BSP_COM_Init(COM1, &com) != BSP_ERROR_NONE)
  {
    Error_Handler();
  }
  BSP_COM_SelectLogPort(COM1);
  setvbuf(stdout, NULL, _IONBF, 0);   /* send each char immediately, no buffering */

  printf("\r\n=== STM32U5A9J-DK: Hello from UART! ===\r\n");
  printf("SYSCLK = %lu Hz\r\n", (unsigned long)HAL_RCC_GetSysClockFreq());

  /* -4- UART receive (interrupt) for the "T YYYY-MM-DD HH:MM:SS" set-time command */
  Uart_Rx_Start();

  /* -5- Real-time clock */
  int rc = rtc_clock_init();
  if (rc != 0)
  {
    printf("RTC init failed: %d\r\n", rc);
  }

  /* -6- LCD + touch + LVGL */
  if (lvgl_port_init() != 0)
  {
    Error_Handler();
  }

  /* -7- STTS22H temperature sensor (I2C3) */
  Temp_Init();

  /* -8- User button (PC13): press = sleep now / wake up */
  BSP_PB_Init(BUTTON_USER, BUTTON_MODE_EXTI);

  /* -9- Sci-fi radar UI (4 swipeable pages) */
  ui_create();

  int32_t temp10 = 0;
  int temp_ok = Temp_Read(&temp10);
  ui_set_temperature(temp_ok, temp10);
  templog_add(temp_ok, temp10);
  uint32_t t_ui = 0, t_temp = HAL_GetTick(), t_log = HAL_GetTick();
  int last_sec = -1;
  rtc_datetime_t dt;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    uint32_t now = HAL_GetTick();

    /* UI animation tick: ~30 Hz (sweep beam, stopwatch) */
    if (now - t_ui >= 33U)
    {
      t_ui = now;
      rtc_clock_get(&dt);
      ui_tick(&dt);

      if (dt.second != last_sec)          /* once per second */
      {
        last_sec = dt.second;
        HAL_GPIO_TogglePin(LED4_GPIO_PORT, LED4_PIN);   /* heartbeat */
        char tb[12];
        ui_fmt_temp(tb, sizeof(tb), temp_ok, temp10, 0);
        printf("%04u-%02u-%02u %02u:%02u:%02u  temp %s  fps %lu\r\n",
               dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second, tb,
               (unsigned long)lvgl_port_take_frames());
      }
    }

    /* temperature: every 2 s on screen, every 10 s into the log */
    if (now - t_temp >= 2000U)
    {
      t_temp = now;
      temp_ok = Temp_Read(&temp10);
      ui_set_temperature(temp_ok, temp10);
    }
    if (now - t_log >= 10000U)
    {
      t_log = now;
      templog_add(temp_ok, temp10);
    }

    /* hourly chime (RTC Alarm B interrupt, every HH:00:00) */
    if (rtc_chime_take_fired())
    {
      rtc_datetime_t c;
      rtc_clock_get(&c);
      ui_chime(c.hour);
    }

    /* daily alarm (RTC Alarm A interrupt) */
    if (rtc_alarm_take_fired())
    {
      uint8_t ah, am; int en;
      rtc_alarm_get(&ah, &am, &en);
      ui_alarm_alert(ah, am);
      lv_display_trigger_activity(NULL);
    }

    /* power saving */
    if (sleep_request ||
        (!ALWAYS_ON && lv_display_get_inactive_time(NULL) > SLEEP_AFTER_MS && !ui_busy()))
    {
      sleep_request = 0;
      Power_Sleep();
      t_ui = 0;
      continue;
    }

    Uart_Poll_Command();
    lv_timer_handler();
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
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  /* HSE (16 MHz) is required: the BSP LCD driver clocks LTDC/DSI from PLL3 sourced by HSE */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI | RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_4;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
  RCC_OscInitStruct.PLL.PLLMBOOST = RCC_PLLMBOOST_DIV1;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 80;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLLVCIRANGE_0;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_PCLK3;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief Power Configuration
  * @retval None
  */
static void SystemPower_Config(void)
{

  /*
   * Disable the internal Pull-Up in Dead Battery pins of UCPD peripheral
   */
  HAL_PWREx_DisableUCPDDeadBattery();

  /*
   * Switch to SMPS regulator instead of LDO
   */
  if (HAL_PWREx_ConfigSupply(PWR_SMPS_SUPPLY) != HAL_OK)
  {
    Error_Handler();
  }
/* USER CODE BEGIN PWR */
/* USER CODE END PWR */
}

/**
  * @brief ICACHE Initialization Function
  * @param None
  * @retval None
  */
static void MX_ICACHE_Init(void)
{

  /* USER CODE BEGIN ICACHE_Init 0 */

  /* USER CODE END ICACHE_Init 0 */

  /* USER CODE BEGIN ICACHE_Init 1 */

  /* USER CODE END ICACHE_Init 1 */

  /** Enable instruction cache in 1-way (direct mapped cache)
  */
  if (HAL_ICACHE_ConfigAssociativityMode(ICACHE_1WAY) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_ICACHE_Enable() != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ICACHE_Init 2 */

  /* USER CODE END ICACHE_Init 2 */

}

/* USER CODE BEGIN 4 */
/* ----------------------------- power saving ------------------------------ */
/* Display off + Stop 2. Wake-up sources: touch panel INT (PE8/EXTI8),
 * USER button (PC13/EXTI13), RTC Alarm A. RTC keeps time, SRAM is retained. */
static const char * volatile wake_reason = "?";

void BSP_PB_Callback(Button_TypeDef Button)
{
  (void)Button;
  if (!in_stop) sleep_request = 1;     /* button while awake: sleep now */
  wake_reason = "button";
}

void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == TS_INT_PIN) wake_reason = "touch";
}

static void Power_Sleep(void)
{
  printf("sleep: display off, Stop 2 (wake: touch / button / alarm)\r\n");
  while (__HAL_UART_GET_FLAG(&hcom_uart[COM1], UART_FLAG_TC) == RESET) { }

  lvgl_port_display_sleep();
  wake_reason = "alarm";
  in_stop = 1;

  HAL_SuspendTick();
  HAL_PWREx_EnterSTOP2Mode(PWR_STOPENTRY_WFI);
  /* ---- woken up: system clock is MSI 4 MHz here ---- */
  SystemClock_Config();
  HAL_ResumeTick();
  in_stop = 0;

  lvgl_port_display_wake();
  printf("wake: %s\r\n", wake_reason);
}

/* --------------------------- temperature --------------------------------- */
static int temp_init_ok;

static void Temp_Init(void)
{
  uint8_t id = 0;
  int32_t err = BSP_ENV_SENSOR_Init(0, ENV_TEMPERATURE);
  if (err == BSP_ERROR_NONE) err = BSP_ENV_SENSOR_Enable(0, ENV_TEMPERATURE);
  if (err == BSP_ERROR_NONE) (void)BSP_ENV_SENSOR_ReadID(0, &id);
  temp_init_ok = (err == BSP_ERROR_NONE);
  printf("Temp sensor (STTS22H) init %s (%ld), ID=0x%02X\r\n",
         temp_init_ok ? "OK" : "FAILED", (long)err, id);
}

static int Temp_Read(int32_t *tenths)
{
  float_t v;
  if (!temp_init_ok || BSP_ENV_SENSOR_GetValue(0, ENV_TEMPERATURE, &v) != BSP_ERROR_NONE)
  {
    return 0;
  }
  *tenths = (int32_t)lroundf(v * 10.0f);
  return 1;
}

/* --------------------- UART command: set the clock ----------------------- */
/* Line format: "T YYYY-MM-DD HH:MM:SS"  (tools/set_time.py sends it)       */
static uint8_t           rx_byte;
static char              rx_line[40];
static volatile uint32_t rx_len;
static char              cmd_line[40];
static volatile int      cmd_ready;

static void Uart_Rx_Start(void)
{
  HAL_NVIC_SetPriority(USART1_IRQn, 6, 0);
  HAL_NVIC_EnableIRQ(USART1_IRQn);
  (void)HAL_UART_Receive_IT(&hcom_uart[COM1], &rx_byte, 1);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart != &hcom_uart[COM1]) return;

  char c = (char)rx_byte;
  if (c == '\r' || c == '\n')
  {
    if (rx_len > 0U && !cmd_ready)
    {
      rx_line[rx_len] = '\0';
      memcpy(cmd_line, rx_line, rx_len + 1U);
      cmd_ready = 1;
    }
    rx_len = 0;
  }
  else if (rx_len < sizeof(rx_line) - 1U)
  {
    rx_line[rx_len++] = c;
  }
  (void)HAL_UART_Receive_IT(&hcom_uart[COM1], &rx_byte, 1);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart != &hcom_uart[COM1]) return;
  rx_len = 0;                                     /* drop the broken line */
  (void)HAL_UART_Receive_IT(&hcom_uart[COM1], &rx_byte, 1);
}

static void Uart_Poll_Command(void)
{
  if (!cmd_ready) return;

  if (cmd_line[0] == 'C' && cmd_line[1] == '\0')       /* "C": test the hourly chime now */
  {
    rtc_datetime_t c;
    rtc_clock_get(&c);
    ui_chime(c.hour);
    printf("OK chime test\r\n");
  }
  else if (rtc_clock_set_from_cmd(cmd_line) == 0)
  {
    printf("OK time set: %s\r\n", cmd_line + 2);
  }
  else
  {
    printf("ERR bad command: '%s' (use: T YYYY-MM-DD HH:MM:SS | C)\r\n", cmd_line);
  }
  cmd_ready = 0;
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @param  None
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  while(1) 
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
  /* Infinite loop */
  while (1)
  {
  }

  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
