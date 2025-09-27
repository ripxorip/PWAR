/*
 * pwar_packet.h - PipeWire ASIO Relay (PWAR) project
 *
 * (c) 2025 Philip K. Gisslow
 * This file is part of the PipeWire ASIO Relay (PWAR) project.
 */

#ifndef PWAR_PACKET
#define PWAR_PACKET

#include <stdint.h>

#define PWAR_PACKET_MAX_CHUNK_SIZE 128
#define PWAR_PACKET_MIN_CHUNK_SIZE 32
#define PWAR_CHANNELS 2

typedef struct {
    uint16_t n_samples; // I.e. the current chunk size, must be <= PWAR_PACKET_MAX_CHUNK_SIZE

    /* 4-point timestamp system for comprehensive latency analysis */
    uint64_t t1_linux_send;
    uint64_t t2_windows_recv;
    uint64_t t3_windows_send;
    uint64_t t4_linux_recv;

    float samples[PWAR_CHANNELS * PWAR_PACKET_MAX_CHUNK_SIZE]; // interleaved samples
} pwar_packet_t;


#endif /* PWAR_PACKET */
