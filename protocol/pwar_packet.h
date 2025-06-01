/*
 * pwar_packet.h - PipeWire ASIO Relay (PWAR) project
 *
 * (c) 2025 Philip K. Gisslow
 * This file is part of the PipeWire ASIO Relay (PWAR) project.
 */

#ifndef RT_STREAM_PACKET_H
#define RT_STREAM_PACKET_H

#include <stdint.h>

#define RT_STREAM_PACKET_FRAME_SIZE 256

typedef struct {
    uint16_t n_samples;
    uint64_t seq;

    uint64_t ts_pipewire_send;     // when PipeWire sends input
    uint64_t ts_asio_send;        // when DAW finishes processing and returns

    float samples_ch1[RT_STREAM_PACKET_FRAME_SIZE/2];
    float samples_ch2[RT_STREAM_PACKET_FRAME_SIZE/2];
} rt_stream_packet_t;

#endif // RT_STREAM_PACKET_H
