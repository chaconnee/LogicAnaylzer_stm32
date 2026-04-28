/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    DmaSampler.c
  * @brief   8通道GPIO DMA采样
  ******************************************************************************
  */
/* USER CODE END Header */

#include "DmaSampler.h"

#include <string.h>

typedef struct {
     __IO uint16_t sampleBuffer[DMA_SAMPLER_BUFFER_SIZE] __attribute__((aligned(4)));
    volatile uint32_t halfReadyCount;
    volatile uint32_t fullReadyCount;
    volatile uint8_t dmaError;
    volatile uint8_t running;
    TIM_HandleTypeDef* htim;
    DMA_HandleTypeDef* hdma;
} DmaSamplerContext_t;

static DmaSamplerContext_t s_ctx;

static uint32_t DmaSampler_EnterCritical(void);
static void DmaSampler_ExitCritical(uint32_t primask);
static uint32_t DmaSampler_GetTim4ClockHz(void);
static void DmaSampler_RegisterCallbacks(void);
static void DmaSampler_HalfCallback(DMA_HandleTypeDef* hdma);
static void DmaSampler_FullCallback(DMA_HandleTypeDef* hdma);
static void DmaSampler_ErrorCallback(DMA_HandleTypeDef* hdma);

static uint32_t DmaSampler_EnterCritical(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    return primask;
}

static void DmaSampler_ExitCritical(uint32_t primask)
{
    if (primask == 0U)
    {
        __enable_irq();
    }
}

static uint32_t DmaSampler_GetTim4ClockHz(void)
{
    uint32_t timClock = HAL_RCC_GetPCLK2Freq();
    //如果 APB1 预分频系数 > 1（即做了分频），则定时器时钟 = PCLK1 × 2
    if ((RCC->CFGR & RCC_CFGR_PPRE2) != RCC_CFGR_PPRE2_DIV1)
    {
        timClock *= 2U;
    }
    return timClock;
}

static void DmaSampler_RegisterCallbacks(void)
{
    if (s_ctx.hdma == NULL)
    {
        return;
    }

    s_ctx.hdma->XferHalfCpltCallback = DmaSampler_HalfCallback;
    s_ctx.hdma->XferCpltCallback = DmaSampler_FullCallback;
    s_ctx.hdma->XferErrorCallback = DmaSampler_ErrorCallback;
}

void DmaSampler_Init(TIM_HandleTypeDef* htim, DMA_HandleTypeDef* hdma)
{
    (void)memset((void*)&s_ctx, 0, sizeof(s_ctx));

    s_ctx.htim = htim;
    s_ctx.hdma = hdma;

    DmaSampler_RegisterCallbacks();
}

void DmaSampler_SetSampleRate(uint32_t sampleRateHz)
{
    uint32_t timClock;
    uint32_t arr;

    if (s_ctx.htim == NULL)
    {
        return;
    }

    if (sampleRateHz == 0U)
    {
        sampleRateHz = LA_RATE_500K_HZ;
    }

    timClock = DmaSampler_GetTim4ClockHz();//96mhz
    if (sampleRateHz > timClock)
    {
        sampleRateHz = timClock;
    }
	//采样率 = TIM1时钟 / (ARR + 1)
    arr = timClock / sampleRateHz;
    if (arr == 0U)
    {
        arr = 1U;
    }
    arr -= 1U;
    if (arr > 0xFFFFU)
    {
        arr = 0xFFFFU;
    }

    HAL_TIM_Base_Stop(s_ctx.htim);
    __HAL_TIM_DISABLE_DMA(s_ctx.htim, TIM_DMA_UPDATE);

    __HAL_TIM_SET_PRESCALER(s_ctx.htim, 0U);
    __HAL_TIM_SET_AUTORELOAD(s_ctx.htim, (uint16_t)arr);
    __HAL_TIM_SET_COUNTER(s_ctx.htim, 0U);
		//软件产生 UG 事件，立即将 ARR 和 PSC 影子寄存器更新到工作寄存器，确保配置即时生效
    s_ctx.htim->Instance->EGR = TIM_EGR_UG;
}

HAL_StatusTypeDef DmaSampler_Start(void)
{
    HAL_StatusTypeDef halStatus;

    if ((s_ctx.hdma == NULL) || (s_ctx.htim == NULL))
    {
        return HAL_ERROR;
    }

    if (s_ctx.running != 0U)
    {
        return HAL_OK;
    }

    DmaSampler_RegisterCallbacks();

    halStatus = HAL_DMA_Start_IT(s_ctx.hdma,
                                 (uint32_t)&GPIOA->IDR,
                                 (uint32_t)s_ctx.sampleBuffer,
                                 DMA_SAMPLER_BUFFER_SIZE);
    if (halStatus != HAL_OK)
    {
        s_ctx.dmaError = 1U;
        return HAL_ERROR;
    }

    __HAL_TIM_ENABLE_DMA(s_ctx.htim, TIM_DMA_UPDATE);
    halStatus = HAL_TIM_Base_Start(s_ctx.htim);
    if (halStatus != HAL_OK)
    {
        __HAL_TIM_DISABLE_DMA(s_ctx.htim, TIM_DMA_UPDATE);
        (void)HAL_DMA_Abort(s_ctx.hdma);
        s_ctx.dmaError = 1U;
        return HAL_ERROR;
    }

    s_ctx.running = 1U;
    return HAL_OK;
}

void DmaSampler_Stop(void)
{
    if ((s_ctx.hdma == NULL) || (s_ctx.htim == NULL))
    {
        return;
    }

    if (s_ctx.running == 0U)
    {
        return;
    }

    HAL_TIM_Base_Stop(s_ctx.htim);
    __HAL_TIM_DISABLE_DMA(s_ctx.htim, TIM_DMA_UPDATE);
    (void)HAL_DMA_Abort(s_ctx.hdma);
    s_ctx.running = 0U;
}

void DmaSampler_ResetCounters(void)
{
    uint32_t primask = DmaSampler_EnterCritical();
    s_ctx.halfReadyCount = 0U;
    s_ctx.fullReadyCount = 0U;
    DmaSampler_ExitCritical(primask);
}

uint32_t DmaSampler_TakeHalfReadyCount(void)
{
    uint32_t primask;
    uint32_t value;

    primask = DmaSampler_EnterCritical();
    value = s_ctx.halfReadyCount;
    s_ctx.halfReadyCount = 0U;
    DmaSampler_ExitCritical(primask);

    return value;
}

uint32_t DmaSampler_TakeFullReadyCount(void)
{
    uint32_t primask;
    uint32_t value;

    primask = DmaSampler_EnterCritical();
    value = s_ctx.fullReadyCount;
    s_ctx.fullReadyCount = 0U;
    DmaSampler_ExitCritical(primask);

    return value;
}

const volatile uint16_t* DmaSampler_GetBuffer(void)
{
    return s_ctx.sampleBuffer;
}

uint16_t DmaSampler_GetBufferSize(void)
{
    return (uint16_t)DMA_SAMPLER_BUFFER_SIZE;
}

uint8_t DmaSampler_HasError(void)
{
    return s_ctx.dmaError;
}

void DmaSampler_ClearError(void)
{
    s_ctx.dmaError = 0U;
}

static void DmaSampler_HalfCallback(DMA_HandleTypeDef* hdma)
{
    if ((s_ctx.hdma == NULL) || (hdma != s_ctx.hdma))
    {
        return;
    }

    if (s_ctx.halfReadyCount < 0xFFFFFFFFU)
    {
        s_ctx.halfReadyCount++;
    }
}

static void DmaSampler_FullCallback(DMA_HandleTypeDef* hdma)
{
    if ((s_ctx.hdma == NULL) || (hdma != s_ctx.hdma))
    {
        return;
    }

    if (s_ctx.fullReadyCount < 0xFFFFFFFFU)
    {
        s_ctx.fullReadyCount++;
    }
}

static void DmaSampler_ErrorCallback(DMA_HandleTypeDef* hdma)
{
    if ((s_ctx.hdma == NULL) || (hdma != s_ctx.hdma))
    {
        return;
    }

    s_ctx.dmaError = 1U;
}
