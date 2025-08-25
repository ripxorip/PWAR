/*
 * pwar_router.h - PipeWire ASIO Relay (PWAR) project
 *
 * (c) 2025 Philip K. Gisslow
 * This file is part of the PipeWire ASIO Relay (PWAR) project.
 */


#ifndef PWAR_ROUTER
#define PWAR_ROUTER

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "pwar_packet.h"

#define PWAR_ROUTER_MAX_CHANNELS 16
#define PWAR_ROUTER_MAX_BUFFER_SIZE 4096

typedef struct {
    uint32_t channel_count;

    float buffers[PWAR_ROUTER_MAX_CHANNELS][PWAR_ROUTER_MAX_BUFFER_SIZE]; // interleaved buffers for each channel

    // State for packet assembly
    uint32_t received_packets;
    uint8_t packet_received[PWAR_ROUTER_MAX_BUFFER_SIZE / PWAR_PACKET_MIN_CHUNK_SIZE];
    uint64_t current_seq; // Track current buffer sequence number
    uint64_t seq_timestamp; // Timestamp for the current sequence
} pwar_router_t;

void pwar_router_init(pwar_router_t *router, uint32_t channel_count);

// Returns the number of samples ready when all packets have been processed, 0 if more packets are needed
// output_buffers: flat array, channel-major order: output_buffers[channel * n_samples + sample]
// max_samples: maximum number of samples per channel to write to output_buffers
int pwar_router_process_packet(pwar_router_t *router, pwar_packet_t *input_packet, float *output_buffers, uint32_t max_samples, uint32_t channel_count);

int pwar_router_process_streaming_packet(pwar_router_t *router, pwar_packet_t *input_packet, float *output_buffers, uint32_t max_samples, uint32_t channel_count);

// samples: flat array, channel-major order: samples[channel * n_samples + sample]
// n_samples: number of samples per channel
// channel_count: number of channels
// packets: output array for generated packets
// packet_count: size of the packets array
// packets_to_send: output, set to the number of packets generated from the input samples
int pwar_router_send_buffer(pwar_router_t *router, uint32_t chunk_size, float *samples, uint32_t n_samples, uint32_t channel_count, pwar_packet_t *packets, const uint32_t packet_count, uint32_t *packets_to_send);

#ifdef __cplusplus
}
#endif
#endif /* PWAR_ROUTER */
