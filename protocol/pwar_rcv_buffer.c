#include <string.h>
#include "pwar_rcv_buffer.h"
#include <stdio.h>

#define PWAR_RCV_BUFFER_MAX_CHANNELS 16
#define PWAR_RCV_BUFFER_MAX_SAMPLES 4096
#define PWAR_RCV_BUFFER_CHUNK_SIZE 128

static struct {
    float buffers[2][PWAR_RCV_BUFFER_MAX_CHANNELS][PWAR_RCV_BUFFER_MAX_SAMPLES];
    uint32_t n_samples[2];
    uint32_t channels;
    uint32_t chunk_pos;
    int buffer_ready[2];
    int ping_pong; // 0 or 1
} rcv = {0};

int pwar_rcv_buffer_add_buffer(float *buffer, uint32_t n_samples, uint32_t channels, uint32_t stride) {
    if (channels > PWAR_RCV_BUFFER_MAX_CHANNELS || n_samples > PWAR_RCV_BUFFER_MAX_SAMPLES) return -1;
    int idx = rcv.ping_pong;
    for (uint32_t ch = 0; ch < channels; ++ch) {
        memcpy(rcv.buffers[idx][ch], &buffer[ch * stride], n_samples * sizeof(float));
    }
    rcv.n_samples[idx] = n_samples;
    rcv.channels = channels;
    rcv.buffer_ready[idx] = 1;
    return 0;
}

int pwar_rcv_get_chunk(float *chunks, uint32_t channels, uint32_t stride) {
    int idx = !rcv.ping_pong; // read from the other buffer
    if (!rcv.buffer_ready[idx] || channels > rcv.channels) {
        // No buffer ready, output silence
        for (uint32_t ch = 0; ch < channels; ++ch) {
            memset(&chunks[ch * stride], 0, PWAR_RCV_BUFFER_CHUNK_SIZE * sizeof(float));
        }
        rcv.ping_pong = !rcv.ping_pong; // swap buffers
        printf("\033[0;31m--- ERROR -- No valid buffer ready, outputting silence\033[0m\n");
        return 0;
    }
    uint32_t n_samples = rcv.n_samples[idx];
    uint32_t start = rcv.chunk_pos * PWAR_RCV_BUFFER_CHUNK_SIZE;
    // Copy chunk
    for (uint32_t ch = 0; ch < channels; ++ch) {
        uint32_t remain = n_samples - start;
        uint32_t to_copy = remain < PWAR_RCV_BUFFER_CHUNK_SIZE ? remain : PWAR_RCV_BUFFER_CHUNK_SIZE;
        memcpy(&chunks[ch * stride], &rcv.buffers[idx][ch][start], to_copy * sizeof(float));
        if (to_copy < PWAR_RCV_BUFFER_CHUNK_SIZE) {
            memset(&chunks[ch * stride + to_copy], 0, (PWAR_RCV_BUFFER_CHUNK_SIZE - to_copy) * sizeof(float));
        }
    }
    rcv.chunk_pos++;
    // If this was the last chunk, mark buffer as consumed
    if ((rcv.chunk_pos * PWAR_RCV_BUFFER_CHUNK_SIZE) >= n_samples) {
        rcv.buffer_ready[idx] = 0;
        rcv.chunk_pos = 0;
        rcv.ping_pong = !rcv.ping_pong; // swap buffers
    }
    return 1;
}