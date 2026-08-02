#include "throughput_calc.h"

uint32_t ThroughputComputeKBps(uint32_t bytes, uint32_t elapsed_ms)
{
    if (elapsed_ms == 0) return 0;
    return (uint32_t)(((uint64_t)bytes * 1000ULL) / elapsed_ms / 1024ULL);
}
