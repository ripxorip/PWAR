/*
 * pwar_packet.h - PipeWire ASIO Relay (PWAR) project
 *
 * (c) 2025 Philip K. Gisslow
 * This file is part of the PipeWire ASIO Relay (PWAR) project.
 */

#ifndef PWAR_PACKET
#define PWAR_PACKET

#include <stdint.h>

#define PWAR_PACKET_CHUNK_SIZE 128
#define PWAR_CHANNELS 2

typedef struct {
    uint16_t n_samples;
    uint64_t seq;

    /* For segmentation */
    uint32_t num_packets;           // total number of packets in this sequence
    uint32_t packet_index;          // index of this packet in the sequence

    uint64_t ts_pipewire_send;      // when PipeWire sends input
    uint64_t ts_asio_send;          // when DAW finishes processing and returns

    float samples[PWAR_CHANNELS][PWAR_PACKET_CHUNK_SIZE]; // interleaved samples
} pwar_packet_t;

#endif /* PWAR_PACKET */
