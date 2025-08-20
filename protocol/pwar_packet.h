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
#define PWAR_PACKET_MIN_CHUNK_SIZE 64
#define PWAR_CHANNELS 2

/*
 * NOTE: This packet structure is currently locked to 2 channels (PWAR_CHANNELS = 2) for simplicity and compatibility.
 * The underlying protocol and codebase are designed to support an arbitrary number of channels in the future.
 *
 * When multi-channel support is implemented, this struct will be refactored to use a flat sample array with stride,
 * rather than a fixed 2D array. This will allow for more flexible and efficient handling of any channel count.
 *
 * If you are developing for more than 2 channels, be aware that this is a planned area of development.
 */

typedef struct {
    uint16_t n_samples; // I.e. the current chunk size, must be <= PWAR_PACKET_MAX_CHUNK_SIZE
    uint64_t seq;

    /* For segmentation */
    uint32_t num_packets;           // total number of packets in this sequence
    uint32_t packet_index;          // index of this packet in the sequence

    uint64_t seq_timestamp;
    uint64_t timestamp;

    float samples[PWAR_CHANNELS][PWAR_PACKET_MAX_CHUNK_SIZE]; // interleaved samples
} pwar_packet_t;

typedef struct {
    uint32_t audio_proc_min; // Minimum processing time in nanoseconds
    uint32_t audio_proc_max; // Maximum processing time in nanoseconds
    uint32_t audio_proc_avg; // Average processing time in nanoseconds

    uint32_t jitter_min; // Minimum network jitter in nanoseconds
    uint32_t jitter_max; // Maximum network jitter in nanoseconds
    uint32_t jitter_avg; // Average network jitter in nanoseconds

} pwar_latency_info_t;

#endif /* PWAR_PACKET */
