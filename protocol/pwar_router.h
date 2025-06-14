/*
 * pwar_router.h - PipeWire ASIO Relay (PWAR) project
 *
 * (c) 2025 Philip K. Gisslow
 * This file is part of the PipeWire ASIO Relay (PWAR) project.
 */

#ifndef PWAR_ROUTER
#define PWAR_ROUTER

#include <stdint.h>
#include "pwar_packet.h"

void pwar_router_init(uint32_t buffer_size, uint32_t channel_count);

// Returns 1 when all packets have been processed, 0 if more packets are needed
int pwar_router_process_packet(pwar_packet_t *input_packet, float **output_buffers, const uint32_t output_size, uint32_t output_channel_count);

// samples: array of pointers to float buffers, one per channel (non-interleaved)
// n_samples: number of samples per channel
// channel_count: number of channels (e.g., 16 for 16-channel audio)
// packets: output array for generated packets
// packet_count: size of the packets array
// packets_to_send: output, set to the number of packets generated from the input samples
int pwar_router_send_buffer(float **samples, uint32_t n_samples, uint32_t channel_count, pwar_packet_t *packets, const uint32_t packet_count, uint32_t *packets_to_send);

#endif /* PWAR_ROUTER */
