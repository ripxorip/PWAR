#ifndef PWAR_SEND_BUFFER
#define PWAR_SEND_BUFFER

#include <stdint.h>

#define PWAR_SEND_BUFFER_MAX_CHANNELS 16
#define PWAR_SEND_BUFFER_MAX_SAMPLES 4096

// Singleton API, no struct needed
// samples: flat array, channel-major order: samples[channel * stride + sample]
// stride: number of samples per channel
void pwar_send_buffer_init(uint32_t channels, uint32_t max_samples);
uint32_t pwar_send_buffer_push(float *samples, uint32_t n_samples, uint32_t channels, uint32_t stride);
int pwar_send_buffer_ready();
void pwar_send_buffer_get(float *out_channels, uint32_t *n_samples, uint32_t stride);

#endif /* PWAR_SEND_BUFFER */
