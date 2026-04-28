/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    DmaSampler.h
  * @brief   8通道GPIO DMA采样（面向过程）
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __DMA_SAMPLER_H
#define __DMA_SAMPLER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

#define DMA_SAMPLER_BUFFER_SIZE  4096U
#define LA_RATE_500K_HZ          500000U
#define LA_RATE_1M_HZ            1000000U

void DmaSampler_Init(TIM_HandleTypeDef* htim, DMA_HandleTypeDef* hdma);
void DmaSampler_SetSampleRate(uint32_t sampleRateHz);
HAL_StatusTypeDef DmaSampler_Start(void);
void DmaSampler_Stop(void);

void DmaSampler_ResetCounters(void);
uint32_t DmaSampler_TakeHalfReadyCount(void);
uint32_t DmaSampler_TakeFullReadyCount(void);

const volatile uint16_t* DmaSampler_GetBuffer(void);
uint16_t DmaSampler_GetBufferSize(void);

uint8_t DmaSampler_HasError(void);
void DmaSampler_ClearError(void);

#ifdef __cplusplus
}
#endif

#endif /* __DMA_SAMPLER_H */
