#ifndef __THROUGHPUT_CALC_H
#define __THROUGHPUT_CALC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* 純邏輯:bytes 在 elapsed_ms 毫秒內傳完,回傳 KB/s。不碰任何硬體/HAL/FatFs,
   可以在電腦上用假數字直接驗證。*/
uint32_t ThroughputComputeKBps(uint32_t bytes, uint32_t elapsed_ms);

#ifdef __cplusplus
}
#endif

#endif /* __THROUGHPUT_CALC_H */
