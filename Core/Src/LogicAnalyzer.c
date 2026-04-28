/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    LogicAnalyzer.c
  * @brief   ???????????????CDC???
  ******************************************************************************
  */
/* USER CODE END Header */

#include "LogicAnalyzer.h"

#include "DmaSampler.h"
#include "usbd_cdc_if.h"

#include <string.h>

#define LOGIC_ANALYZER_HEADER0      0xAAU
#define LOGIC_ANALYZER_HEADER1      0x55U

#define LOGIC_ANALYZER_SEQ_SIZE     4U
#define LOGIC_ANALYZER_LENGTH_SIZE  2U
#define LOGIC_ANALYZER_CHECK_SIZE   1U
#define LOGIC_ANALYZER_OVERHEAD     (2U + LOGIC_ANALYZER_LENGTH_SIZE + LOGIC_ANALYZER_SEQ_SIZE + LOGIC_ANALYZER_CHECK_SIZE)
#define LOGIC_ANALYZER_MAX_PAYLOAD  (LOGIC_ANALYZER_TX_BUFFER_SIZE - LOGIC_ANALYZER_OVERHEAD)
#define LOGIC_ANALYZER_MAX_SAMPLES  LOGIC_ANALYZER_MAX_PAYLOAD

static uint8_t s_txBuffer[LOGIC_ANALYZER_TX_BUFFER_SIZE];
static uint8_t s_cmdBuffer[LOGIC_ANALYZER_CMD_BUFFER_SIZE];
static uint8_t s_cmdLength = 0U;

static volatile uint32_t s_frameSequence = 0U;
static volatile uint32_t s_droppedPacketCount = 0U;
static volatile MODE s_mode = LOGIC_ANALYZER_MODE_IDLE;

static uint8_t LogicAnalyzer_ToLower(uint8_t value);
static uint8_t LogicAnalyzer_StringEqualsIgnoreCase(const uint8_t* buffer,
                                                    uint8_t length,
                                                    const char* token);
static uint8_t LogicAnalyzer_ParseCommand(const uint8_t* buffer, uint8_t length);
static uint16_t LogicAnalyzer_BuildAndSendPacket(const volatile uint16_t* samples,
                                                uint16_t sampleCount,
                                                uint32_t sequence);

void LogicAnalyzer_Init(void)
{
    (void)memset(s_txBuffer, 0, sizeof(s_txBuffer));
    (void)memset(s_cmdBuffer, 0, sizeof(s_cmdBuffer));
    s_cmdLength = 0U;
    s_frameSequence = 0U;
    s_droppedPacketCount = 0U;
    s_mode = LOGIC_ANALYZER_MODE_IDLE;
}

void LogicAnalyzer_ProcessByte(uint8_t rxByte)
{
    uint8_t cmd;

    if ((rxByte == '\n') || (rxByte == '\r'))
    {
        if (s_cmdLength > 0U)
        {
            cmd = LogicAnalyzer_ParseCommand(s_cmdBuffer, s_cmdLength);

            if (cmd == 1U)
            {
                s_mode = LOGIC_ANALYZER_MODE_STREAM_500K;
                s_frameSequence = 0U;
            }
            else if (cmd == 2U)
            {
                s_mode = LOGIC_ANALYZER_MODE_IDLE;
            }
            else if (cmd == 3U)
            {
                s_mode = LOGIC_ANALYZER_MODE_STREAM_1M;
                s_frameSequence = 0U;
            }
        }

        s_cmdLength = 0U;
        return;
    }

    if (s_cmdLength < (LOGIC_ANALYZER_CMD_BUFFER_SIZE - 1U))
    {
        s_cmdBuffer[s_cmdLength++] = rxByte;
    }
    else
    {
        s_cmdLength = 0U;
    }
}

uint16_t LogicAnalyzer_TransmitSamples(const volatile uint16_t* samples,
                                      uint16_t sampleCount)
{
    uint16_t offset;
    uint16_t remaining;
    uint32_t sequence;

    if ((samples == NULL) || (sampleCount == 0U))
    {
        return 0U;
    }

    if (s_mode == LOGIC_ANALYZER_MODE_IDLE)
    {
        return 0U;
    }

    sequence = s_frameSequence++;
    offset = 0U;
    remaining = sampleCount;

    while (remaining > 0U)
    {
        uint16_t chunk = (remaining > LOGIC_ANALYZER_MAX_SAMPLES) ?
                         (uint16_t)LOGIC_ANALYZER_MAX_SAMPLES : remaining;

        if (LogicAnalyzer_BuildAndSendPacket(&samples[offset], chunk, sequence) == 0U)
        {
            return 0U;
        }

        offset += chunk;
        remaining -= chunk;
    }

    return 1U;
}

//????????s_mode????????main.c????
MODE LogicAnalyzer_GetMode(void)
{
    return s_mode;
}

uint32_t LogicAnalyzer_GetRateForMode(MODE mode)
{
    if (mode == LOGIC_ANALYZER_MODE_STREAM_1M)
    {
        return LA_RATE_1M_HZ;
    }

    return LA_RATE_500K_HZ;
}

void LogicAnalyzer_ForceStop(void)
{
    s_mode = LOGIC_ANALYZER_MODE_IDLE;
}

uint32_t LogicAnalyzer_GetDroppedPacketCount(void)
{
    return s_droppedPacketCount;
}

uint32_t LogicAnalyzer_GetFrameSequence(void)
{
    return s_frameSequence;
}

static uint8_t LogicAnalyzer_ToLower(uint8_t value)
{
    if ((value >= 'A') && (value <= 'Z'))
    {
        return (uint8_t)(value + ('a' - 'A'));
    }

    return value;
}

static uint8_t LogicAnalyzer_StringEqualsIgnoreCase(const uint8_t* buffer,
                                                    uint8_t length,
                                                    const char* token)
{
    uint8_t tokenLen = 0U;
    uint8_t i;

    while (token[tokenLen] != '\0')
    {
        tokenLen++;
    }

    if (length != tokenLen)
    {
        return 0U;
    }

    for (i = 0U; i < length; i++)
    {
        if (LogicAnalyzer_ToLower(buffer[i]) != LogicAnalyzer_ToLower((uint8_t)token[i]))
        {
            return 0U;
        }
    }

    return 1U;
}

static uint8_t LogicAnalyzer_ParseCommand(const uint8_t* buffer, uint8_t length)
{
    if ((length == 1U) && (buffer[0] == '1'))
    {
        return 1U;
    }

    if ((length == 1U) && (buffer[0] == '2'))
    {
        return 2U;
    }

    if ((length == 1U) && (buffer[0] == '3'))
    {
        return 3U;
    }

    if (LogicAnalyzer_StringEqualsIgnoreCase(buffer, length, "start") != 0U)
    {
        return 1U;
    }

    if (LogicAnalyzer_StringEqualsIgnoreCase(buffer, length, "stop") != 0U)
    {
        return 2U;
    }

    if ((LogicAnalyzer_StringEqualsIgnoreCase(buffer, length, "high") != 0U) ||
        (LogicAnalyzer_StringEqualsIgnoreCase(buffer, length, "1m") != 0U) ||
        (LogicAnalyzer_StringEqualsIgnoreCase(buffer, length, "fast") != 0U))
    {
        return 3U;
    }

    return 0U;
}
/**
		???  ????             ????  ???
		????????????????????????????????????????????????????????????????????????????????????????
		[0]   0xAA             1B    ??0????????
		[1]   0x55             1B    ??1????????
		[2]  payloadLength_L   1B    ?????????? (?????)
		[3]  payloadLength_H   1B    ?????????? (?????)
		[4]  sequence[0]       1B    ???? (byte 0)
		[5]  sequence[1]       1B    ???? (byte 1)
		[6]  sequence[2]       1B    ???? (byte 2)
		[7]  sequence[3]       1B    ???? (byte 3)
		[8]  samples[0]        1B    ??1????????
		[9]  samples[1]        1B    ??2????????
		...  ...               ...   ...
		[8+chunk-1] samples[N] 1B    ???1????????
		[8+chunk]   checksum   1B    ????????

**/
static uint16_t LogicAnalyzer_BuildAndSendPacket(const volatile uint16_t* samples,
                                                uint16_t sampleCount,
                                                uint32_t sequence)
{
    uint16_t idx = 0U;
    uint16_t i;
    uint16_t payloadLength;
    uint8_t checksum = 0U;

    s_txBuffer[idx++] = LOGIC_ANALYZER_HEADER0;
    s_txBuffer[idx++] = LOGIC_ANALYZER_HEADER1;

    payloadLength = (uint16_t)(LOGIC_ANALYZER_SEQ_SIZE + sampleCount);
    s_txBuffer[idx++] = (uint8_t)(payloadLength & 0xFFU);
    s_txBuffer[idx++] = (uint8_t)((payloadLength >> 8) & 0xFFU);
    checksum ^= s_txBuffer[idx - 2U];
    checksum ^= s_txBuffer[idx - 1U];

    s_txBuffer[idx++] = (uint8_t)(sequence & 0xFFU);
    s_txBuffer[idx++] = (uint8_t)((sequence >> 8) & 0xFFU);
    s_txBuffer[idx++] = (uint8_t)((sequence >> 16) & 0xFFU);
    s_txBuffer[idx++] = (uint8_t)((sequence >> 24) & 0xFFU);
    checksum ^= s_txBuffer[idx - 4U];
    checksum ^= s_txBuffer[idx - 3U];
    checksum ^= s_txBuffer[idx - 2U];
    checksum ^= s_txBuffer[idx - 1U];

    for (i = 0U; i < sampleCount; i++)
    {
        /* DMA以halfword(16bit)读取GPIOA->IDR, 但只需PA0-PA7低8位, 屏蔽高8位 */
        uint8_t sample = (uint8_t)(samples[i] & 0xFFU);
        s_txBuffer[idx++] = sample;
        checksum ^= sample;
    }

    s_txBuffer[idx++] = checksum;

    if (CDC_Transmit_FS(s_txBuffer, idx) != USBD_OK)
    {
        s_droppedPacketCount++;
        return 0U;
    }

    return 1U;
}
