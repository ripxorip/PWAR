// Simulated Audio Backend for PWAR
// Provides perfect timing simulation for testing without hardware

#define _GNU_SOURCE
#include "audio_backend.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

// Simulated backend private data
typedef struct {
    pthread_t thread;
    volatile int running;
    audio_process_callback_t callback;
    void *userdata;
    
    // Audio configuration
    uint32_t sample_rate;
    uint32_t frames;
    uint32_t channels_in;
    uint32_t channels_out;
    
    // Test signal generation
    double phase;
    double freq;        // Low frequency for latency measurement
    
    // Timing statistics
    uint64_t total_callbacks;
    struct timespec start_time;

    uint64_t last_input_zero_cross;
    uint64_t last_output_zero_cross;
    float rtt;

    // RTT stats for last 2 seconds
    float rtt_min;
    float rtt_max;
    double rtt_sum;
    uint32_t rtt_count;
    uint32_t discontinuities;
} simulated_backend_data_t;

static uint64_t timespec_to_ns(const struct timespec *ts) {
    return (uint64_t)ts->tv_sec * 1000000000ULL + ts->tv_nsec;
}

// Get the current time in nanoseconds
static uint64_t get_current_time_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return timespec_to_ns(&ts);
}

static void perform_analysis(simulated_backend_data_t *data, float *input_buffer, float *output_left, float *output_right, uint32_t frames) {
    // Print every value in left output buffer for debugging
    static float last_input_sample = 0.0f;
    static float last_output_sample = 0.0f;

    static double output_phase = 0.0;
    const float epsilon = 1e-4f; // Acceptable error margin

    for (uint32_t i = 0; i < frames; i++) {
        // Input zero crossing: could be used for sync, but we just track RTT as before
        if (input_buffer[i] >= 0.0f && last_input_sample < 0.0f) {
            data->last_input_zero_cross = get_current_time_ns();
        }
        if (output_left[i] >= 0.0f && last_output_sample < 0.0f) {
            output_phase = 0.0; // Reset output phase on zero crossing
            data->last_output_zero_cross = get_current_time_ns();
            data->rtt = (data->last_output_zero_cross - data->last_input_zero_cross) / 1000000.0f; // in ms

            // Update RTT stats
            if (data->rtt_count == 0) {
                data->rtt_min = data->rtt_max = data->rtt;
                data->rtt_sum = data->rtt;
            } else {
                if (data->rtt < data->rtt_min) data->rtt_min = data->rtt;
                if (data->rtt > data->rtt_max) data->rtt_max = data->rtt;
                data->rtt_sum += data->rtt;
            }
            data->rtt_count++;
        }

        // For every sample, verify output_left matches expected sine value
        double expected_sample = 0.3 * sin(2.0 * M_PI * output_phase);
        if (fabsf(output_left[i] - expected_sample) > epsilon) {
            data->discontinuities++;
        }

        output_phase += data->freq / data->sample_rate;
        if (output_phase >= 1.0) output_phase -= 1.0;

        last_input_sample = input_buffer[i];
        last_output_sample = output_left[i];
    }
}

static void* simulated_thread(void* arg) {
    audio_backend_t *backend = (audio_backend_t *)arg;
    simulated_backend_data_t *data = (simulated_backend_data_t*)backend->private_data;
    
    printf("[Simulated Audio] Starting audio simulation thread\n");
    printf("[Simulated Audio] Sample rate: %u Hz, Buffer size: %u frames\n", 
           data->sample_rate, data->frames);
    printf("[Simulated Audio] Test signal: %.1f Hz\n", data->freq);
    
    // Calculate precise timing for buffer delivery
    uint64_t frame_time_ns = (uint64_t)data->frames * 1000000000ULL / data->sample_rate;
    struct timespec sleep_time = {
        .tv_sec = frame_time_ns / 1000000000ULL,
        .tv_nsec = frame_time_ns % 1000000000ULL
    };
    
    printf("[Simulated Audio] Buffer interval: %.3f ms\n", frame_time_ns / 1000000.0);
    
    // Allocate buffers (single channel input)
    float *input_buffer = calloc(data->frames, sizeof(float));
    float *output_left = calloc(data->frames, sizeof(float));
    float *output_right = calloc(data->frames, sizeof(float));

    if (!input_buffer || !output_left || !output_right) {
        printf("[Simulated Audio] Failed to allocate buffers\n");
        goto cleanup;
    }

    // Record start time
    clock_gettime(CLOCK_MONOTONIC, &data->start_time);

    // Initialize RTT stats
    data->rtt_min = 0.0f;
    data->rtt_max = 0.0f;
    data->rtt_sum = 0.0;
    data->rtt_count = 0;

    while (data->running) {
        // Generate test input signal (single channel sine wave)
        for (uint32_t i = 0; i < data->frames; i++) {
            float sample = 0.3f * sinf(2.0f * M_PI * data->phase);
            input_buffer[i] = sample;
            data->phase += data->freq / data->sample_rate;
            if (data->phase >= 1.0) data->phase -= 1.0;
        }

        // Call the audio processing callback (PWAR protocol processing)
        if (data->callback) {
            data->callback(input_buffer, output_left, output_right, data->frames, data->userdata);
        }

        data->total_callbacks++;

        perform_analysis(data, input_buffer, output_left, output_right, data->frames);

        // Print periodic RTT stats (every 2nd second)
        if (data->total_callbacks % (2 * data->sample_rate / data->frames) == 0) {
            float rtt_avg = (data->rtt_count > 0) ? (float)(data->rtt_sum / data->rtt_count) : 0.0f;
            printf("[Simulated Audio]: AudioProc: RTT: min=%.3fms max=%.3fms avg=%.3fms\n",
                   data->rtt_min, data->rtt_max, rtt_avg);

            if (data->discontinuities > 0) {
                printf("\033[1;31m[Simulated Audio] ERROR: Detected %u discontinuities in output signal over last 2 seconds\033[0m\n", data->discontinuities);
            }

            // Reset stats for next interval
            data->rtt_min = 0.0f;
            data->rtt_max = 0.0f;
            data->rtt_sum = 0.0;
            data->rtt_count = 0;
            data->discontinuities = 0;
        }

        // Simulate precise hardware timing
        nanosleep(&sleep_time, NULL);
    }
    
cleanup:
    printf("[Simulated Audio] Stopping audio simulation thread\n");
    free(input_buffer);
    free(output_left);
    free(output_right);
    return NULL;
}

static int simulated_init(audio_backend_t *backend, const audio_config_t *config,
                         audio_process_callback_t callback, void *userdata) {
    simulated_backend_data_t *data = calloc(1, sizeof(simulated_backend_data_t));
    if (!data) {
        printf("[Simulated Audio] Failed to allocate backend data\n");
        return -1;
    }
    
    data->callback = callback;
    data->userdata = userdata;
    data->sample_rate = config->sample_rate;
    data->frames = config->frames;
    data->channels_in = config->capture_channels;
    data->channels_out = config->playback_channels;
    data->running = 0;
    data->total_callbacks = 0;
    
    // Test signal frequency (low frequency for latency measurement)
    // At 10 Hz, zero crossings are ~100ms apart, good for measuring 0.8-30ms latency
    data->freq = 10.0;    // 10 Hz sine wave
    data->phase = 0.0;
    
    backend->private_data = data;
    backend->callback = callback;
    backend->userdata = userdata;
    
    printf("[Simulated Audio] Backend initialized successfully\n");
    return 0;
}

static int simulated_start(audio_backend_t *backend) {
    simulated_backend_data_t *data = (simulated_backend_data_t*)backend->private_data;
    if (!data) return -1;
    
    if (data->running) {
        printf("[Simulated Audio] Already running\n");
        return 0;
    }
    
    data->running = 1;
    backend->running = 1;
    
    if (pthread_create(&data->thread, NULL, simulated_thread, backend) != 0) {
        printf("[Simulated Audio] Failed to create audio thread\n");
        data->running = 0;
        backend->running = 0;
        return -1;
    }
    
    printf("[Simulated Audio] Started successfully\n");
    return 0;
}

static int simulated_stop(audio_backend_t *backend) {
    simulated_backend_data_t *data = (simulated_backend_data_t*)backend->private_data;
    if (!data || !data->running) return 0;
    
    printf("[Simulated Audio] Stopping...\n");
    data->running = 0;
    backend->running = 0;
    
    pthread_join(data->thread, NULL);
    printf("[Simulated Audio] Stopped successfully\n");
    return 0;
}

static void simulated_cleanup(audio_backend_t *backend) {
    simulated_backend_data_t *data = (simulated_backend_data_t*)backend->private_data;
    if (!data) return;
    
    if (data->running) {
        simulated_stop(backend);
    }
    
    printf("[Simulated Audio] Cleaning up\n");
    free(data);
    backend->private_data = NULL;
}

static int simulated_is_running(audio_backend_t *backend) {
    simulated_backend_data_t *data = (simulated_backend_data_t*)backend->private_data;
    return data ? data->running : 0;
}

static void simulated_get_stats(audio_backend_t *backend, void *stats) {
    simulated_backend_data_t *data = (simulated_backend_data_t*)backend->private_data;
    if (!data || !stats) return;
    
    // For now, just print some basic stats
    printf("[Simulated Audio Stats] Total callbacks: %llu\n", 
           (unsigned long long)data->total_callbacks);
}

static float simulated_get_latency(audio_backend_t *backend) {
    simulated_backend_data_t *data = (simulated_backend_data_t *)backend->private_data;
    if (!data || !data->sample_rate) return 0.0f;
    return ((float)data->frames * 2.0f * 1000.0f) / (float)data->sample_rate;
}

static const audio_backend_ops_t simulated_ops = {
    .init = simulated_init,
    .start = simulated_start,
    .stop = simulated_stop,
    .cleanup = simulated_cleanup,
    .is_running = simulated_is_running,
    .get_stats = simulated_get_stats,
    .get_latency = simulated_get_latency
};

audio_backend_t* audio_backend_create_simulated(void) {
    audio_backend_t *backend = calloc(1, sizeof(audio_backend_t));
    if (!backend) {
        printf("[Simulated Audio] Failed to allocate backend\n");
        return NULL;
    }
    
    backend->ops = &simulated_ops;
    backend->private_data = NULL;
    backend->running = 0;
    
    return backend;
}
