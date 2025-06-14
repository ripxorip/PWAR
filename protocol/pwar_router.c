/*
 * pwar_router.c - PipeWire <-> UDP streaming bridge for PWAR
 *
 * (c) 2025 Philip K. Gisslow
 * This file is part of the PipeWire ASIO Relay (PWAR) project.
 */

#include "pwar_router.h"
#include "pwar_packet.h"

#define PWAR_ROUTER_MAX_CHANNELS 16
#define PWAR_ROUTER_MAX_BUFFER_SIZE 4096

static struct {
    uint32_t buffer_size;
    uint32_t channel_count;

    float buffers[PWAR_ROUTER_MAX_CHANNELS][PWAR_ROUTER_MAX_BUFFER_SIZE]; // interleaved buffers for each channel

    // State for packet assembly
    uint32_t received_packets;
    uint8_t packet_received[PWAR_ROUTER_MAX_BUFFER_SIZE / PWAR_PACKET_CHUNK_SIZE];
} internal = {0};

void pwar_router_init(uint32_t buffer_size, uint32_t channel_count) {
    internal.buffer_size = buffer_size;
    internal.channel_count = channel_count;
    internal.received_packets = 0;
    for (uint32_t i = 0; i < PWAR_ROUTER_MAX_BUFFER_SIZE / PWAR_PACKET_CHUNK_SIZE; ++i) internal.packet_received[i] = 0;
}

int pwar_router_process_packet(pwar_packet_t *input_packet, float **output_buffers, const uint32_t output_size, uint32_t output_channel_count) {
    if (!input_packet || !output_buffers) return -1;
    if (input_packet->num_packets == 0 || input_packet->packet_index >= input_packet->num_packets) return -2;
    if (input_packet->n_samples > PWAR_PACKET_CHUNK_SIZE) return -3;
    // Only reset state when starting a new buffer (packet_index == 0)
    if (input_packet->packet_index == 0) {
        internal.received_packets = 0;
        for (uint32_t i = 0; i < PWAR_ROUTER_MAX_BUFFER_SIZE / PWAR_PACKET_CHUNK_SIZE; ++i) internal.packet_received[i] = 0;
    }
    if (!internal.packet_received[input_packet->packet_index]) {
        // Copy samples to internal buffer
        uint32_t offset = input_packet->packet_index * PWAR_PACKET_CHUNK_SIZE;
        for (uint32_t ch = 0; ch < internal.channel_count && ch < PWAR_CHANNELS; ++ch) {
            for (uint32_t s = 0; s < input_packet->n_samples && (offset + s) < internal.buffer_size; ++s) {
                internal.buffers[ch][offset + s] = input_packet->samples[ch][s];
            }
        }
        internal.packet_received[input_packet->packet_index] = 1;
        internal.received_packets++;
    }
    // Check if all packets for this buffer are received
    if (internal.received_packets == input_packet->num_packets) {
        uint32_t n_samples = internal.buffer_size < output_size ? internal.buffer_size : output_size;
        for (uint32_t ch = 0; ch < output_channel_count && ch < internal.channel_count; ++ch) {
            for (uint32_t s = 0; s < n_samples; ++s) {
                output_buffers[ch][s] = internal.buffers[ch][s];
            }
        }
        return 1; // Output ready
    }
    return 0; // Not ready yet
}

// Returns 0 on success, -1 if not enough space in packets array, -2 if invalid arguments
int pwar_router_send_buffer(float **samples, uint32_t n_samples, uint32_t channel_count, pwar_packet_t *packets, const uint32_t packet_count, uint32_t *packets_to_send) {
    if (!samples || !packets || !packets_to_send || channel_count == 0 || n_samples == 0) return -2;
    uint32_t chunk_size = PWAR_PACKET_CHUNK_SIZE;
    uint32_t total_samples = (n_samples < internal.buffer_size) ? n_samples : internal.buffer_size;
    uint32_t total_packets = (total_samples + chunk_size - 1) / chunk_size;
    if (total_packets > packet_count) {
        *packets_to_send = 0;
        return -1; // Not enough space in packets array
    }
    for (uint32_t p = 0; p < total_packets; ++p) {
        uint32_t offset = p * chunk_size;
        uint32_t samples_in_packet = (offset + chunk_size <= total_samples) ? chunk_size : (total_samples - offset);
        for (uint32_t ch = 0; ch < channel_count; ++ch) {
            for (uint32_t s = 0; s < samples_in_packet; ++s) {
                packets[p].samples[ch][s] = samples[ch][offset + s];
            }
        }
        packets[p].num_packets = total_packets;
        packets[p].packet_index = p;
        packets[p].n_samples = samples_in_packet; // <-- FIX: set n_samples for each packet
    }
    *packets_to_send = total_packets;
    return 0;
}

