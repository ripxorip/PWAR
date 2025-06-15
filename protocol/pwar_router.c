/*
 * pwar_router.c - PipeWire <-> UDP streaming bridge for PWAR
 *
 * (c) 2025 Philip K. Gisslow
 * This file is part of the PipeWire ASIO Relay (PWAR) project.
 */

#include "pwar_router.h"
#include "pwar_packet.h"
#include <string.h>

void pwar_router_init(pwar_router_t *router, uint32_t channel_count) {
    router->channel_count = channel_count;
    router->received_packets = 0;
    for (uint32_t i = 0; i < PWAR_ROUTER_MAX_BUFFER_SIZE / PWAR_PACKET_CHUNK_SIZE; ++i) router->packet_received[i] = 0;
}

int pwar_router_process_packet(pwar_router_t *router, pwar_packet_t *input_packet, float *output_buffers, const uint32_t output_size, uint32_t channel_count, uint32_t stride) {
    if (!input_packet || !output_buffers) return -1;
    if (input_packet->num_packets == 0 || input_packet->packet_index >= input_packet->num_packets) return -2;
    if (input_packet->n_samples > PWAR_PACKET_CHUNK_SIZE) return -3;
    // Only reset state when starting a new buffer (packet_index == 0)
    if (input_packet->packet_index == 0) {
        router->received_packets = 0;
        for (uint32_t i = 0; i < PWAR_ROUTER_MAX_BUFFER_SIZE / PWAR_PACKET_CHUNK_SIZE; ++i) router->packet_received[i] = 0;
    }
    if (!router->packet_received[input_packet->packet_index]) {
        // Copy samples to internal buffer
        uint32_t offset = input_packet->packet_index * PWAR_PACKET_CHUNK_SIZE;
        for (uint32_t ch = 0; ch < router->channel_count && ch < PWAR_CHANNELS; ++ch) {
            for (uint32_t s = 0; s < input_packet->n_samples; ++s) {
                router->buffers[ch][offset + s] = input_packet->samples[ch][s];
            }
        }
        router->packet_received[input_packet->packet_index] = 1;
        router->received_packets++;
    }
    // Check if all packets for this buffer are received
    if (router->received_packets == input_packet->num_packets) {
        // Calculate total number of samples from packet info
        uint32_t total_samples = (input_packet->num_packets - 1) * PWAR_PACKET_CHUNK_SIZE + input_packet->n_samples;
        uint32_t n_samples = total_samples < output_size ? total_samples : output_size;
        for (uint32_t ch = 0; ch < channel_count && ch < router->channel_count; ++ch) {
            memcpy(&output_buffers[ch * stride], router->buffers[ch], n_samples * sizeof(float));
        }
        return 1; // Output ready
    }
    return 0; // Not ready yet
}

// Returns 0 on success, -1 if not enough space in packets array, -2 if invalid arguments
int pwar_router_send_buffer(pwar_router_t *router, float *samples, uint32_t n_samples, uint32_t channel_count, uint32_t stride, pwar_packet_t *packets, const uint32_t packet_count, uint32_t *packets_to_send) {
    (void)router; // Unused in this implementation, but could be used for future enhancements
    if (!samples || !packets || !packets_to_send || channel_count == 0 || n_samples == 0) return -2;
    uint32_t chunk_size = PWAR_PACKET_CHUNK_SIZE;
    uint32_t total_packets = (n_samples + chunk_size - 1) / chunk_size;
    if (total_packets > packet_count) {
        *packets_to_send = 0;
        return -1; // Not enough space in packets array
    }
    for (uint32_t p = 0; p < total_packets; ++p) {
        uint32_t start = p * chunk_size;
        uint32_t ns = (n_samples - start > chunk_size) ? chunk_size : (n_samples - start);
        packets[p].packet_index = p;
        packets[p].num_packets = total_packets;
        packets[p].n_samples = ns;
        for (uint32_t ch = 0; ch < channel_count; ++ch) {
            memcpy(packets[p].samples[ch], &samples[ch * stride + start], ns * sizeof(float));
        }
    }
    *packets_to_send = total_packets;
    return 1;
}

