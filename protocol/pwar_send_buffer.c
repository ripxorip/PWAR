#include "pwar_send_buffer.h"
#include <string.h>

// Singleton send buffer instance
static struct {
    float buffer[PWAR_SEND_BUFFER_MAX_CHANNELS][PWAR_SEND_BUFFER_MAX_SAMPLES];
    uint32_t channels;
    uint32_t n_samples;
    uint32_t max_samples;
} sb = {0};

void pwar_send_buffer_init(uint32_t channels, uint32_t max_samples) {
    sb.channels = channels;
    sb.n_samples = 0;
    sb.max_samples = max_samples;
    for (uint32_t ch = 0; ch < channels; ++ch) {
        memset(sb.buffer[ch], 0, sizeof(float) * max_samples);
    }
}

uint32_t pwar_send_buffer_push(float **samples, uint32_t n_samples) {
    if (sb.n_samples + n_samples > sb.max_samples) n_samples = sb.max_samples - sb.n_samples;
    for (uint32_t ch = 0; ch < sb.channels; ++ch) {
        memcpy(&sb.buffer[ch][sb.n_samples], samples[ch], n_samples * sizeof(float));
    }
    sb.n_samples += n_samples;
    return sb.n_samples;
}

int pwar_send_buffer_ready() {
    return sb.n_samples >= sb.max_samples;
}

void pwar_send_buffer_get(float **out_channels, uint32_t *n_samples) {
    if (out_channels) {
        for (uint32_t ch = 0; ch < sb.channels; ++ch) {
            out_channels[ch] = sb.buffer[ch];
        }
    }
    if (n_samples) *n_samples = sb.n_samples;
    // Automatically reset after reading out samples
    sb.n_samples = 0;
}
