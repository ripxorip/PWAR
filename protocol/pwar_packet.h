#ifndef RT_STREAM_PACKET_H
#define RT_STREAM_PACKET_H

#include <stdint.h>

#define RT_STREAM_PACKET_FRAME_SIZE 256

typedef struct {
    uint16_t n_samples;
    uint64_t seq;
    uint64_t timestamp;
    int16_t samples[RT_STREAM_PACKET_FRAME_SIZE];
} rt_stream_packet_t;

#endif // RT_STREAM_PACKET_H%
