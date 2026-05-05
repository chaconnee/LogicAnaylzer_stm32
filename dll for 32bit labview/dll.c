
#include <stdint.h>


#ifdef BUILD_DLL
    #define DLL_EXPORT __declspec(dllexport)
#else
    #define DLL_EXPORT __declspec(dllimport)
#endif


#define LOGIC_HEADER0    0xAA
#define LOGIC_HEADER1    0x55
#define SEQ_SIZE         4
#define CHANNEL_COUNT    8


DLL_EXPORT int ParseLogicAnalyzerData(const unsigned char* raw_data, int raw_len, unsigned char* out_channel_data, int max_samples, int* parsed_sample_count, int* consumed_bytes)
{
    // 参数有效性检查
    if (!raw_data || raw_len <= 0 || !out_channel_data || max_samples <= 0 || !parsed_sample_count || !consumed_bytes) return 0;
    
    int parsed = 0;
    int idx = 0;

    // 至少需要5个字节才能构成有效的最短包
    while (idx <= raw_len - 5) {
        if (raw_data[idx] == LOGIC_HEADER0 && raw_data[idx + 1] == LOGIC_HEADER1) {
            if (idx + 4 > raw_len) break;
            
            uint16_t payload_length = (uint16_t)raw_data[idx + 2] | ((uint16_t)raw_data[idx + 3] << 8);
            if (payload_length < SEQ_SIZE) { idx++; continue; }
            
            int total_packet_len = 4 + payload_length + 1;
            if (idx + total_packet_len > raw_len) break;

            uint8_t checksum = 0;
            for (int j = 2; j < total_packet_len - 1; j++) checksum ^= raw_data[idx + j];
            if (checksum != raw_data[idx + total_packet_len - 1]) { idx++; continue; }

            int sample_count = (int)payload_length - SEQ_SIZE;
            int copy_count = sample_count;
            int overflow = 0;
            if (parsed + sample_count > max_samples) {
                copy_count = max_samples - parsed;
                overflow = 1;
            }

            for (int s = 0; s < copy_count; s++) {
                uint8_t sample_byte = raw_data[idx + 8 + s];
                // LabVIEW 二维数组结构 (Row-Major)
                for (int ch = 0; ch < CHANNEL_COUNT; ch++) {
                    out_channel_data[ch * max_samples + parsed] = (sample_byte >> ch) & 0x01;
                }
                parsed++;
            }
            idx += total_packet_len;
            
            if (overflow) {
                *parsed_sample_count = parsed;
                *consumed_bytes = idx;
                return 2;
            }
        } else {
            idx++;
        }
    }
    *parsed_sample_count = parsed;
    *consumed_bytes = idx;
    return 1;
}

