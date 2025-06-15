#ifndef PWAR_RCV_BUFFER
#define PWAR_RCV_BUFFER

#include <stdint.h>

// No init needed, always static
// buffer: flat array, channel-major order: buffer[channel * stride + sample]
// stride: number of samples per channel
int pwar_rcv_buffer_add_buffer(float *buffer, uint32_t n_samples, uint32_t channels, uint32_t stride);
int pwar_rcv_get_chunk(float *chunks, uint32_t channels, uint32_t stride);

#endif /* PWAR_RCV_BUFFER */
