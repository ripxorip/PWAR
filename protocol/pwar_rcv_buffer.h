#ifndef PWAR_RCV_BUFFER
#define PWAR_RCV_BUFFER

#include <stdint.h>

// No init needed, always static
int pwar_rcv_buffer_add_buffer(float **buffer, uint32_t n_samples, uint32_t channels);
int pwar_rcv_get_chunk(float **chunks, uint32_t channels);

#endif /* PWAR_RCV_BUFFER */
