// ALSA Audio Backend for PWAR
// Provides ALSA-specific audio interface implementation

#include "audio_backend.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <sys/time.h>
#include <alsa/asoundlib.h>
#include <pthread.h>
#include <signal.h>

// ALSA-specific statistics
typedef struct {
    unsigned long total_iterations;
    unsigned long capture_xruns;
    unsigned long playback_xruns;
    double total_loop_time;
    double min_loop_time;
    double max_loop_time;
    struct timeval start_time;
} alsa_stats_t;

// ALSA backend private data
typedef struct {
    snd_pcm_t *playback_handle;
    snd_pcm_t *capture_handle;
    audio_config_t config;
    
    // Audio buffers
    int32_t *playback_buffer;
    int32_t *capture_buffer;
    float *input_float_buffer;
    float *output_left_buffer;
    float *output_right_buffer;
    
    // Threading
    pthread_t audio_thread;
    volatile int should_stop;
    
    // Statistics
    alsa_stats_t stats;

    // Latency measurement
    float latency_ms;
} alsa_backend_data_t;

static double get_ms_elapsed(const struct timeval *t0, const struct timeval *t1) {
    return (t1->tv_sec - t0->tv_sec) * 1000.0 + 
           (t1->tv_usec - t0->tv_usec) / 1000.0;
}

// Convert int32_t samples from ALSA to float
static void int32_to_float(int32_t *input, float *output, int samples) {
    for (int i = 0; i < samples; i++) {
        output[i] = (float)input[i] / 2147483648.0f;  // 2^31
    }
}

// Convert float samples to int32_t for ALSA
static void float_to_int32(float *input, int32_t *output, int samples) {
    for (int i = 0; i < samples; i++) {
        // Clamp to [-1.0, 1.0] and convert to int32
        float clamped = fmaxf(-1.0f, fminf(1.0f, input[i]));
        output[i] = (int32_t)(clamped * 2147483647.0f);  // 2^31 - 1
    }
}

static int setup_pcm(snd_pcm_t **handle,
                     const char *device,
                     snd_pcm_stream_t stream,
                     unsigned int rate,
                     unsigned int channels,
                     snd_pcm_uframes_t period,
                     alsa_backend_data_t *data)
{
    int err;
    snd_pcm_hw_params_t *hw;
    snd_pcm_sw_params_t *sw;

    if ((err = snd_pcm_open(handle, device, stream, 0)) < 0) {
        fprintf(stderr, "%s open error: %s\n",
                stream == SND_PCM_STREAM_PLAYBACK ? "Playback" : "Capture",
                snd_strerror(err));
        return err;
    }

    // Hardware parameters
    snd_pcm_hw_params_alloca(&hw);
    snd_pcm_hw_params_any(*handle, hw);
    snd_pcm_hw_params_set_access(*handle, hw, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(*handle, hw, SND_PCM_FORMAT_S32_LE);
    snd_pcm_hw_params_set_channels(*handle, hw, channels);

    unsigned int rr = rate;
    int dir = 0;
    snd_pcm_hw_params_set_rate_near(*handle, hw, &rr, &dir);

    snd_pcm_uframes_t per = period;
    snd_pcm_hw_params_set_period_size_near(*handle, hw, &per, &dir);

    snd_pcm_uframes_t buf = per * 2;  // 2 periods for slightly more safety
    snd_pcm_hw_params_set_buffer_size_near(*handle, hw, &buf);

    if ((err = snd_pcm_hw_params(*handle, hw)) < 0) {
        fprintf(stderr, "hw_params error: %s\n", snd_strerror(err));
        return err;
    }

    // Software parameters - important for xrun behavior
    snd_pcm_sw_params_alloca(&sw);
    snd_pcm_sw_params_current(*handle, sw);
    
    // Start threshold
    snd_pcm_sw_params_set_start_threshold(*handle, sw, per);
    
    // Minimum available frames to wake up
    snd_pcm_sw_params_set_avail_min(*handle, sw, per);
    
    if ((err = snd_pcm_sw_params(*handle, sw)) < 0) {
        fprintf(stderr, "sw_params error: %s\n", snd_strerror(err));
        return err;
    }

    snd_pcm_prepare(*handle);

    // Print configuration
    snd_pcm_hw_params_get_rate(hw, &rr, &dir);
    snd_pcm_hw_params_get_period_size(hw, &per, &dir);
    snd_pcm_hw_params_get_buffer_size(hw, &buf);

    float latency = rr ? ((float)buf * 1000.0f / rr) : 0.0f;
    printf("ALSA %s: %u Hz, %u ch, period=%lu, buffer=%lu (%.2f ms buffer)\n",
           stream == SND_PCM_STREAM_PLAYBACK ? "Playback" : "Capture",
           rr, channels, (unsigned long)per, (unsigned long)buf,
           latency);
    if (data) {
        data->latency_ms += latency;
    }

    return 0;
}

static void *alsa_audio_thread(void *arg) {
    audio_backend_t *backend = (audio_backend_t *)arg;
    alsa_backend_data_t *data = (alsa_backend_data_t *)backend->private_data;
    
    struct timeval loop_start, loop_end;
    unsigned long clean_loops = 0;
    
    gettimeofday(&data->stats.start_time, NULL);
    data->stats.min_loop_time = 999999;
    
    printf("\nStarting ALSA audio processing thread. Press Ctrl+C for statistics.\n");
    printf("Legend: . = 1000 clean loops, C = capture xrun, P = playback xrun\n\n");
    
    while (!data->should_stop) {
        gettimeofday(&loop_start, NULL);
        
        // 1) Capture
        int err = snd_pcm_readi(data->capture_handle, data->capture_buffer, data->config.frames);
        if (err == -EPIPE || err == -ESTRPIPE) {
            printf("C");
            fflush(stdout);
            data->stats.capture_xruns++;
            snd_pcm_prepare(data->capture_handle);
            continue;
        } else if (err < 0) {
            fprintf(stderr, "\nCapture error: %s\n", snd_strerror(err));
            snd_pcm_prepare(data->capture_handle);
            continue;
        }

        // 2) Convert to float and process
        int32_to_float(data->capture_buffer, data->input_float_buffer, 
                      data->config.frames * data->config.capture_channels);

        // Extract input samples (guitar is on right channel if stereo)
        float *mono_input = data->input_float_buffer;
        if (data->config.capture_channels > 1) {
            // Extract right channel for guitar input
            for (uint32_t i = 0; i < data->config.frames; i++) {
                mono_input[i] = data->input_float_buffer[i * data->config.capture_channels + 1];
            }
        }

        // Call the PWAR processing callback
        if (backend->callback) {
            backend->callback(mono_input, data->output_left_buffer, data->output_right_buffer,
                            data->config.frames, backend->userdata);
        }

        // 3) Convert back to int32 and playback
        for (uint32_t i = 0; i < data->config.frames; i++) {
            // Convert left channel
            float clamped_left = fmaxf(-1.0f, fminf(1.0f, data->output_left_buffer[i]));
            data->playback_buffer[i * data->config.playback_channels + 0] = 
                (int32_t)(clamped_left * 2147483647.0f);
            
            // Convert right channel
            if (data->config.playback_channels > 1) {
                float clamped_right = fmaxf(-1.0f, fminf(1.0f, data->output_right_buffer[i]));
                data->playback_buffer[i * data->config.playback_channels + 1] = 
                    (int32_t)(clamped_right * 2147483647.0f);
            }
        }

        err = snd_pcm_writei(data->playback_handle, data->playback_buffer, data->config.frames);
        if (err == -EPIPE || err == -ESTRPIPE) {
            printf("P");
            fflush(stdout);
            data->stats.playback_xruns++;
            snd_pcm_prepare(data->playback_handle);
            continue;
        } else if (err < 0) {
            fprintf(stderr, "\nPlayback error: %s\n", snd_strerror(err));
            snd_pcm_prepare(data->playback_handle);
            continue;
        }

        // Update statistics
        gettimeofday(&loop_end, NULL);
        double loop_time = get_ms_elapsed(&loop_start, &loop_end);
        
        data->stats.total_loop_time += loop_time;
        if (loop_time < data->stats.min_loop_time) data->stats.min_loop_time = loop_time;
        if (loop_time > data->stats.max_loop_time) data->stats.max_loop_time = loop_time;
        
        data->stats.total_iterations++;
        clean_loops++;
        
        // Progress indicator every 1000 clean loops
        if (0 && (clean_loops >= 1000)) {
            printf(".");
            fflush(stdout);
            clean_loops = 0;
        }
    }
    
    return NULL;
}

static int alsa_init(audio_backend_t *backend, const audio_config_t *config,
                    audio_process_callback_t callback, void *userdata) {
    alsa_backend_data_t *data = malloc(sizeof(alsa_backend_data_t));
    if (!data) {
        return -1;
    }
    
    memset(data, 0, sizeof(alsa_backend_data_t));
    data->config = *config;
    backend->private_data = data;
    backend->callback = callback;
    backend->userdata = userdata;
    
    // Allocate audio buffers
    data->playback_buffer = calloc(config->frames * config->playback_channels, sizeof(int32_t));
    data->capture_buffer = calloc(config->frames * config->capture_channels, sizeof(int32_t));
    data->input_float_buffer = calloc(config->frames * config->capture_channels, sizeof(float));
    data->output_left_buffer = calloc(config->frames, sizeof(float));
    data->output_right_buffer = calloc(config->frames, sizeof(float));
    
    if (!data->playback_buffer || !data->capture_buffer || !data->input_float_buffer ||
        !data->output_left_buffer || !data->output_right_buffer) {
        alsa_backend_data_t *d = data;
        free(d->playback_buffer);
        free(d->capture_buffer);
        free(d->input_float_buffer);
        free(d->output_left_buffer);
        free(d->output_right_buffer);
        free(data);
        return -1;
    }
    
    // Setup ALSA devices
    data->latency_ms = 0.0f;
    if (setup_pcm(&data->playback_handle, config->device_playback, SND_PCM_STREAM_PLAYBACK,
                  config->sample_rate, config->playback_channels, config->frames, data) < 0) {
        return -1;
    }
    
    if (setup_pcm(&data->capture_handle, config->device_capture, SND_PCM_STREAM_CAPTURE,
                  config->sample_rate, config->capture_channels, config->frames, data) < 0) {
        snd_pcm_close(data->playback_handle);
        return -1;
    }
    
    return 0;
}

static int alsa_start(audio_backend_t *backend) {
    alsa_backend_data_t *data = (alsa_backend_data_t *)backend->private_data;
    if (!data || backend->running) {
        return -1;
    }
    
    data->should_stop = 0;
    if (pthread_create(&data->audio_thread, NULL, alsa_audio_thread, backend) != 0) {
        return -1;
    }
    
    backend->running = 1;
    return 0;
}

static int alsa_stop(audio_backend_t *backend) {
    alsa_backend_data_t *data = (alsa_backend_data_t *)backend->private_data;
    if (!data || !backend->running) {
        return -1;
    }
    
    data->should_stop = 1;
    pthread_join(data->audio_thread, NULL);
    backend->running = 0;
    return 0;
}

static void alsa_cleanup(audio_backend_t *backend) {
    alsa_backend_data_t *data = (alsa_backend_data_t *)backend->private_data;
    if (!data) {
        return;
    }
    
    if (backend->running) {
        alsa_stop(backend);
    }
    
    // Print final statistics
    struct timeval now;
    gettimeofday(&now, NULL);
    double runtime = (now.tv_sec - data->stats.start_time.tv_sec) + 
                    (now.tv_usec - data->stats.start_time.tv_usec) / 1000000.0;
    
    printf("\n========== ALSA Statistics ==========\n");
    printf("Runtime: %.1f seconds\n", runtime);
    printf("Total iterations: %lu\n", data->stats.total_iterations);
    printf("Capture XRUNs: %lu (%.3f%%)\n", 
           data->stats.capture_xruns, 
           data->stats.total_iterations > 0 ? (100.0 * data->stats.capture_xruns / data->stats.total_iterations) : 0);
    printf("Playback XRUNs: %lu (%.3f%%)\n", 
           data->stats.playback_xruns,
           data->stats.total_iterations > 0 ? (100.0 * data->stats.playback_xruns / data->stats.total_iterations) : 0);
    
    if (data->stats.total_iterations > 0) {
        double avg_loop = data->stats.total_loop_time / data->stats.total_iterations;
        printf("Loop time: avg=%.3f ms, min=%.3f ms, max=%.3f ms\n",
               avg_loop, data->stats.min_loop_time, data->stats.max_loop_time);
        printf("Theoretical min latency: %.3f ms (%.1f samples @ %d Hz)\n",
               (double)data->config.frames * 1000.0 / data->config.sample_rate, 
               (double)data->config.frames, data->config.sample_rate);
    }
    printf("====================================\n");
    
    // Check final device state
    if (data->capture_handle) {
        snd_pcm_state_t cap_state = snd_pcm_state(data->capture_handle);
        printf("Final capture state: %s\n", snd_pcm_state_name(cap_state));
        snd_pcm_close(data->capture_handle);
    }
    
    if (data->playback_handle) {
        snd_pcm_state_t pb_state = snd_pcm_state(data->playback_handle);
        printf("Final playback state: %s\n", snd_pcm_state_name(pb_state));
        snd_pcm_close(data->playback_handle);
    }
    
    free(data->playback_buffer);
    free(data->capture_buffer);
    free(data->input_float_buffer);
    free(data->output_left_buffer);
    free(data->output_right_buffer);
    free(data);
    backend->private_data = NULL;
}

static int alsa_is_running(audio_backend_t *backend) {
    return backend->running;
}

static void alsa_get_stats(audio_backend_t *backend, void *stats) {
    alsa_backend_data_t *data = (alsa_backend_data_t *)backend->private_data;
    if (data && stats) {
        memcpy(stats, &data->stats, sizeof(alsa_stats_t));
    }
}

// Return the sum of playback and capture buffer latencies in ms
static float alsa_get_latency(audio_backend_t *backend) {
    alsa_backend_data_t *data = (alsa_backend_data_t *)backend->private_data;
    if (!data) return 0.0f;
    return data->latency_ms;
}

static const audio_backend_ops_t alsa_ops = {
    .init = alsa_init,
    .start = alsa_start,
    .stop = alsa_stop,
    .cleanup = alsa_cleanup,
    .is_running = alsa_is_running,
    .get_stats = alsa_get_stats,
    .get_latency = alsa_get_latency
};

audio_backend_t* audio_backend_create_alsa(void) {
    audio_backend_t *backend = malloc(sizeof(audio_backend_t));
    if (!backend) {
        return NULL;
    }
    
    memset(backend, 0, sizeof(audio_backend_t));
    backend->ops = &alsa_ops;
    return backend;
}
