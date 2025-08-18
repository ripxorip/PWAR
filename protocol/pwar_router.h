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

#define PWAR_ROUTER_MAX_CHANNELS 16
#define PWAR_ROUTER_MAX_BUFFER_SIZE 4096

typedef struct {
    uint32_t channel_count;

    float buffers[PWAR_ROUTER_MAX_CHANNELS][PWAR_ROUTER_MAX_BUFFER_SIZE]; // interleaved buffers for each channel

    // State for packet assembly
    uint32_t received_packets;
    uint8_t packet_received[PWAR_ROUTER_MAX_BUFFER_SIZE / PWAR_PACKET_CHUNK_SIZE];
    uint64_t current_seq; // Track current buffer sequence number
} pwar_router_t;

void pwar_router_init(pwar_router_t *router, uint32_t channel_count);

// Returns 1 when all packets have been processed, 0 if more packets are needed
// output_buffers: flat array, channel-major order: output_buffers[channel * stride + sample]
// output_size: number of samples per channel
// stride: number of samples per channel
int pwar_router_process_packet(pwar_router_t *router, pwar_packet_t *input_packet, float *output_buffers, const uint32_t output_size, uint32_t channel_count, uint32_t stride);

int pwar_router_process_streaming_packet(pwar_router_t *router, pwar_packet_t *input_packet, float *output_buffers, const uint32_t output_size, uint32_t channel_count, uint32_t stride);

// samples: flat array, channel-major order: samples[channel * stride + sample]
// n_samples: number of samples per channel
// channel_count: number of channels
// stride: number of samples per channel
// packets: output array for generated packets
// packet_count: size of the packets array
// packets_to_send: output, set to the number of packets generated from the input samples
int pwar_router_send_buffer(pwar_router_t *router, float *samples, uint32_t n_samples, uint32_t channel_count, uint32_t stride, pwar_packet_t *packets, const uint32_t packet_count, uint32_t *packets_to_send);

#endif /* PWAR_ROUTER */
