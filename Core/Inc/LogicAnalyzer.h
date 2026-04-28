/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    LogicAnalyzer.h
  * @brief   逻辑分析仪命令与CDC打包（面向过程）
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __LOGIC_ANALYZER_H
#define __LOGIC_ANALYZER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define LOGIC_ANALYZER_TX_BUFFER_SIZE   2048U
#define LOGIC_ANALYZER_CMD_BUFFER_SIZE  32U

typedef enum {
    LOGIC_ANALYZER_MODE_IDLE = 0U,
    LOGIC_ANALYZER_MODE_STREAM_500K = 1U,
    LOGIC_ANALYZER_MODE_STREAM_1M = 2U
} MODE;

void LogicAnalyzer_Init(void);
void LogicAnalyzer_ProcessByte(uint8_t rxByte);

uint16_t LogicAnalyzer_TransmitSamples(const volatile uint16_t* samples,
                                      uint16_t sampleCount);

MODE LogicAnalyzer_GetMode(void);
uint32_t LogicAnalyzer_GetRateForMode(MODE mode);
void LogicAnalyzer_ForceStop(void);

uint32_t LogicAnalyzer_GetDroppedPacketCount(void);
uint32_t LogicAnalyzer_GetFrameSequence(void);

#ifdef __cplusplus
}
#endif

#endif /* __LOGIC_ANALYZER_H */
