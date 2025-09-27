
#ifndef LATENCY_MANAGER
#define LATENCY_MANAGER

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "pwar_packet.h"
#include "pwar_latency_types.h"

void latency_manager_init(uint32_t sample_rate, uint32_t buffer_size, float audio_backend_latency_ms);
uint64_t latency_manager_timestamp_now();

void latency_manager_process_packet(pwar_packet_t *packet);

void latency_manager_report_ring_buffer_fill_level(uint32_t fill_level);

void latency_manger_get_current_metrics(pwar_latency_metrics_t *metrics);

#ifdef __cplusplus
}
#endif
#endif /* LATENCY_MANAGER */
