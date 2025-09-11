#ifndef PWAR_RING_BUFFER
#define PWAR_RING_BUFFER

#include <stdint.h>
#include <pthread.h>

void pwar_ring_buffer_init(uint32_t depth, uint32_t channels, uint32_t expected_buffer_size);
void pwar_ring_buffer_free();

int pwar_ring_buffer_push(float *buffer, uint32_t n_samples, uint32_t channels);
int pwar_ring_buffer_pop(float *samples, uint32_t n_samples, uint32_t channels);

// Additional utility functions
uint32_t pwar_ring_buffer_get_available();
uint32_t pwar_ring_buffer_get_overruns();
uint32_t pwar_ring_buffer_get_underruns();
void pwar_ring_buffer_reset_stats();

#endif /* PWAR_RING_BUFFER */
