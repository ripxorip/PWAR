#include "pwar_ring_buffer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>


static struct {
    float *buffer;              // Audio buffer
    uint32_t depth;             // Buffer depth in samples
    uint32_t channels;          // Number of audio channels
    uint32_t expected_buffer_size; // Expected buffer size for prefill
    uint32_t write_index;       // Current write position
    uint32_t read_index;        // Current read position
    uint32_t available;         // Number of samples available to read
    uint32_t overruns;          // Count of overrun events
    uint32_t underruns;         // Count of underrun events
    pthread_mutex_t mutex;      // Thread safety
} ring_buffer = {0};


static void prefill_buffer() {
    if (ring_buffer.buffer == NULL) {
        return;
    }
    
    // Reset indices and fill with zeros up to depth - expected_buffer_size
    // This leaves one expected_buffer_size worth of headroom for overruns
    ring_buffer.write_index = 0;
    ring_buffer.read_index = 0;
    
    // Prefill to depth - expected_buffer_size, leaving headroom
    uint32_t prefill_samples = ring_buffer.depth - ring_buffer.expected_buffer_size;
    for (uint32_t i = 0; i < prefill_samples; i++) {
        uint32_t write_pos = ring_buffer.write_index * ring_buffer.channels;
        for (uint32_t ch = 0; ch < ring_buffer.channels; ch++) {
            ring_buffer.buffer[write_pos + ch] = 0.0f;
        }
        ring_buffer.write_index = (ring_buffer.write_index + 1) % ring_buffer.depth;
    }

    ring_buffer.available = prefill_samples; // Buffer has prefill_samples available
}


void pwar_ring_buffer_init(uint32_t depth, uint32_t channels, uint32_t expected_buffer_size) {
    // Free any existing buffer
    if (ring_buffer.buffer != NULL) {
        pwar_ring_buffer_free();
    }

    // Initialize the ring buffer structure with increased depth to handle overruns
    ring_buffer.depth = depth + expected_buffer_size;
    ring_buffer.channels = channels;
    ring_buffer.expected_buffer_size = expected_buffer_size;
    ring_buffer.write_index = 0;
    ring_buffer.read_index = 0;
    ring_buffer.available = 0;
    ring_buffer.overruns = 0;
    ring_buffer.underruns = 0;
    
    // Allocate buffer memory (total_depth * channels * sizeof(float))
    ring_buffer.buffer = (float*)calloc(ring_buffer.depth * channels, sizeof(float));
    if (ring_buffer.buffer == NULL) {
        fprintf(stderr, "Error: Failed to allocate ring buffer memory\n");
        return;
    }
    
    // Initialize mutex for thread safety
    if (pthread_mutex_init(&ring_buffer.mutex, NULL) != 0) {
        fprintf(stderr, "Error: Failed to initialize ring buffer mutex\n");
        free(ring_buffer.buffer);
        ring_buffer.buffer = NULL;
        return;
    }
    
    prefill_buffer();
    printf("Ring buffer initialized: %d samples + %d buffer (%d total), %d channels\n", 
           depth, expected_buffer_size, ring_buffer.depth, channels);
}

void pwar_ring_buffer_free() {
    pthread_mutex_lock(&ring_buffer.mutex);
    
    if (ring_buffer.buffer != NULL) {
        free(ring_buffer.buffer);
        ring_buffer.buffer = NULL;
    }
    
    ring_buffer.depth = 0;
    ring_buffer.channels = 0;
    ring_buffer.expected_buffer_size = 0;
    ring_buffer.write_index = 0;
    ring_buffer.read_index = 0;
    ring_buffer.available = 0;
    
    pthread_mutex_unlock(&ring_buffer.mutex);
    pthread_mutex_destroy(&ring_buffer.mutex);
    
    printf("Ring buffer freed\n");
}

int pwar_ring_buffer_push(float *buffer, uint32_t n_samples, uint32_t channels) {
    if (ring_buffer.buffer == NULL || buffer == NULL) {
        return -1; // Buffer not initialized or invalid input
    }
    
    if (channels != ring_buffer.channels) {
        fprintf(stderr, "Error: Channel count mismatch (%d vs %d)\n", channels, ring_buffer.channels);
        return -1;
    }
    
    pthread_mutex_lock(&ring_buffer.mutex);
    
    // Check for potential overrun
    uint32_t samples_to_write = n_samples;
    uint32_t free_space = ring_buffer.depth - ring_buffer.available;
    
    if (samples_to_write > free_space) {
        // Overrun detected - we'll overwrite the oldest data
        ring_buffer.overruns++;
        
        // Move read pointer forward to make space for new data
        uint32_t samples_to_skip = samples_to_write - free_space;
        ring_buffer.read_index = (ring_buffer.read_index + samples_to_skip) % ring_buffer.depth;
        
        printf("Warning: Ring buffer overrun detected. Skipped %d samples (total overruns: %d)\n", 
               samples_to_skip, ring_buffer.overruns);
    }
    
    // Copy samples to the ring buffer
    for (uint32_t i = 0; i < samples_to_write; i++) {
        uint32_t write_pos = ring_buffer.write_index * ring_buffer.channels;
        
        // Copy all channels for this sample
        for (uint32_t ch = 0; ch < ring_buffer.channels; ch++) {
            ring_buffer.buffer[write_pos + ch] = buffer[i * channels + ch];
        }
        
        ring_buffer.write_index = (ring_buffer.write_index + 1) % ring_buffer.depth;
    }
    
    // Update available count: if overrun occurred, buffer is now full
    if (samples_to_write > free_space) {
        ring_buffer.available = ring_buffer.depth; // Buffer is full after overrun
    } else {
        ring_buffer.available += samples_to_write; // Normal case
    }
    
    pthread_mutex_unlock(&ring_buffer.mutex);
    
    return 1; // Success
}

int pwar_ring_buffer_pop(float *samples, uint32_t n_samples, uint32_t channels) {
    if (ring_buffer.buffer == NULL || samples == NULL) {
        return -1; // Buffer not initialized or invalid output
    }
    
    if (channels != ring_buffer.channels) {
        fprintf(stderr, "Error: Channel count mismatch (%d vs %d)\n", channels, ring_buffer.channels);
        return -1;
    }
    
    pthread_mutex_lock(&ring_buffer.mutex);
    
    uint32_t samples_to_read = n_samples;
    
    // Check for underrun
    if (samples_to_read > ring_buffer.available) {
        ring_buffer.underruns++;
        
        printf("Warning: Ring buffer underrun detected. Requested %d, available %d (total underruns: %d)\n",
               n_samples, ring_buffer.available, ring_buffer.underruns);
        
        // Fill output with zeros since we have no valid data
        memset(samples, 0, n_samples * channels * sizeof(float));
        
        // Reset the entire buffer with zeros for maximum protection against future underruns
        prefill_buffer();
        
        pthread_mutex_unlock(&ring_buffer.mutex);
        return n_samples; // Return the requested number (filled with zeros)
    }
    
    // Copy samples from the ring buffer
    for (uint32_t i = 0; i < samples_to_read; i++) {
        uint32_t read_pos = ring_buffer.read_index * ring_buffer.channels;
        
        // Copy all channels for this sample
        for (uint32_t ch = 0; ch < ring_buffer.channels; ch++) {
            samples[i * channels + ch] = ring_buffer.buffer[read_pos + ch];
        }
        
        ring_buffer.read_index = (ring_buffer.read_index + 1) % ring_buffer.depth;
    }
    
    ring_buffer.available -= samples_to_read;
    
    pthread_mutex_unlock(&ring_buffer.mutex);
    
    return samples_to_read; // Return actual number of samples read
}

uint32_t pwar_ring_buffer_get_available() {
    pthread_mutex_lock(&ring_buffer.mutex);
    uint32_t available = ring_buffer.available;
    pthread_mutex_unlock(&ring_buffer.mutex);
    return available;
}

uint32_t pwar_ring_buffer_get_overruns() {
    pthread_mutex_lock(&ring_buffer.mutex);
    uint32_t overruns = ring_buffer.overruns;
    pthread_mutex_unlock(&ring_buffer.mutex);
    return overruns;
}

uint32_t pwar_ring_buffer_get_underruns() {
    pthread_mutex_lock(&ring_buffer.mutex);
    uint32_t underruns = ring_buffer.underruns;
    pthread_mutex_unlock(&ring_buffer.mutex);
    return underruns;
}

void pwar_ring_buffer_reset_stats() {
    pthread_mutex_lock(&ring_buffer.mutex);
    ring_buffer.overruns = 0;
    ring_buffer.underruns = 0;
    pthread_mutex_unlock(&ring_buffer.mutex);
}