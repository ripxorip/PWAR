
#ifndef LATENCY_MANAGER
#define LATENCY_MANAGER

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "pwar_packet.h"
#include "pwar_latency_types.h"

void latency_manager_init();
uint64_t latency_manager_timestamp_now();

void latency_manager_process_packet_client(pwar_packet_t *packet);
void latency_manager_process_packet_server(pwar_packet_t *packet);

void latency_manager_start_audio_cbk_begin();
void latency_manager_start_audio_cbk_end();

int latency_manager_time_for_sending_latency_info(pwar_latency_info_t *latency_info);
void latency_manager_handle_latency_info(pwar_latency_info_t *latency_info);

// New function to get current metrics for GUI display
void latency_manager_get_current_metrics(pwar_latency_metrics_t *metrics);

void latency_manager_report_xrun();

#ifdef __cplusplus
}
#endif
#endif /* LATENCY_MANAGER */
