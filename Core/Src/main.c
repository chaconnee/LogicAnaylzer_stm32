/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : 8通道逻辑分析仪
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "DmaSampler.h"
#include "LogicAnalyzer.h"
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
TIM_HandleTypeDef htim1;
DMA_HandleTypeDef hdma_tim1_up;

/* USER CODE BEGIN PV */
static MODE s_prevMode = LOGIC_ANALYZER_MODE_IDLE;

volatile uint32_t debug_dma_error_code = 0;
volatile uint32_t debug_dma_error_dir = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_TIM1_Init(void);
/* USER CODE BEGIN PFP */
static void MainLoopStep(void);  //主循环调度器
static void HandleModeChange(MODE mode);  //模式切换逻辑
static void StreamPendingData(void);  //数据传输编排

/* USER CODE END PFP */

/**
  * @brief  应用程序入口
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */
	 SCB->VTOR = FLASH_BASE | 0x4000;
	//1
//__HAL_RCC_GPIOA_CLK_DISABLE();
//__HAL_RCC_GPIOC_CLK_DISABLE();
  HAL_RCC_DeInit();
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  HAL_Init();

  /* USER CODE BEGIN Init */
  /* USER CODE END Init */

  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  /* USER CODE END SysInit */

  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USB_DEVICE_Init();
  MX_TIM1_Init();

  /* USER CODE BEGIN 2 */
  DmaSampler_Init(&htim1, &hdma_tim1_up);
  LogicAnalyzer_Init();

  /* 默认采样率配置为500kHz，等待上位机命令启动。 */
  DmaSampler_SetSampleRate(LA_RATE_500K_HZ);
	
  /* USER CODE END 2 */
  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    MainLoopStep();
	
    /* USER CODE END 3 */
  }
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
	

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 192;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                              | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{
  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 0;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 191;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }

  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }

  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{
  __HAL_RCC_DMA2_CLK_ENABLE();


  HAL_NVIC_SetPriority(DMA2_Stream5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream5_IRQn);
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /* PA0..PA7: 8路输入 */
  GPIO_InitStruct.Pin = GPIO_CH0_Pin | GPIO_CH1_Pin | GPIO_CH2_Pin | GPIO_CH3_Pin |
                        GPIO_CH4_Pin | GPIO_CH5_Pin | GPIO_CH6_Pin | GPIO_CH7_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* PC13: 可选状态灯 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
}

/* USER CODE BEGIN 4 */
void OnUsbByte(uint8_t rxByte)
{
  LogicAnalyzer_ProcessByte(rxByte);
}


//模式切换执行器，完成 DMA 采样器的启停和重配置
static void MainLoopStep(void)
{
  MODE mode = LogicAnalyzer_GetMode(); //运行状态枚举0,1,2

  /* 1) 检测命令导致的模式变化，并执行一次性切换动作。 */
  if (mode != s_prevMode)
  {
    HandleModeChange(mode);

    /* 切换函数内部可能触发强制停止，这里重新读取真实模式。 */
    mode = LogicAnalyzer_GetMode();
    s_prevMode = mode;
  }  //使用 s_prevMode 记录上一次的稳态模式，确保模式切换动作只执行一次，而不是每轮循环都重复执行。

  /* 2）采样中 → 将 DMA 半/全缓冲区的数据转发到 CDC 虚拟串口 */
  if (mode != LOGIC_ANALYZER_MODE_IDLE)
  {
    StreamPendingData();
  }
	//空闲态 → 清除 DMA 回调遗留的标志位，防止下次启动时误触发传输
  else
  {
    (void)DmaSampler_TakeHalfReadyCount();
    (void)DmaSampler_TakeFullReadyCount();
  }

  /* 3) 统一错误处理。 */
  if (DmaSampler_HasError() != 0U)
  {
    DmaSampler_Stop();  //停止dma和定时器
    DmaSampler_ClearError();  //清除错误标志
    LogicAnalyzer_ForceStop();  //强制切回IDIE模式（“0”）（不采样）
    s_prevMode = LOGIC_ANALYZER_MODE_IDLE;  //同步稳态记录
  }
}

static void HandleModeChange(MODE mode)
{
  if (mode == LOGIC_ANALYZER_MODE_IDLE)
  {
    DmaSampler_Stop();
    return;
  }

  /* 先停后配，确保500k/1M切换时DMA一定按新配置重启。 */
  DmaSampler_Stop();
  DmaSampler_ResetCounters();
  DmaSampler_ClearError();
  DmaSampler_SetSampleRate(LogicAnalyzer_GetRateForMode(mode));

  if (DmaSampler_Start() != HAL_OK)
  {
    DmaSampler_Stop();
    LogicAnalyzer_ForceStop();
  }
}

static void StreamPendingData(void)
{
  const volatile uint16_t* buffer;
  uint16_t halfSize;
  uint32_t halfCount;
  uint32_t fullCount;

  buffer = DmaSampler_GetBuffer(); //获取缓冲区指针
  if (buffer == NULL)
  {
    return;
  }

  halfSize = (uint16_t)(DmaSampler_GetBufferSize() / 2U);
  halfCount = DmaSampler_TakeHalfReadyCount(); //获取并清零 halfReadyCount（半缓冲就绪次数）
  fullCount = DmaSampler_TakeFullReadyCount();  //获取并清零 fullReadyCount（全缓冲就绪次数）

 
//程序走不进循环（在上位机发送1\n的时候）
  while (halfCount > 0U)
  {
    halfCount--;
    (void)LogicAnalyzer_TransmitSamples(buffer, halfSize);
  }

  while (fullCount > 0U)
  {
    fullCount--;
    (void)LogicAnalyzer_TransmitSamples(&buffer[halfSize], halfSize);
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
  * @param  line: assert_param error line number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  (void)file;
  (void)line;
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
