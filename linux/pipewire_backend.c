// PipeWire Audio Backend for PWAR
// Provides PipeWire-specific audio interface implementation

#include "audio_backend.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <spa/pod/builder.h>
#include <spa/param/latency-utils.h>
#include <pipewire/pipewire.h>
#include <pipewire/filter.h>

// PipeWire backend private data
typedef struct {
    struct pw_main_loop *loop;
    struct pw_filter *filter;
    struct pw_filter_port *in_port;
    struct pw_filter_port *left_out_port;
    struct pw_filter_port *right_out_port;
    
    audio_config_t config;
    pthread_t pw_thread;
    volatile int should_stop;
} pipewire_backend_data_t;

struct port {
    struct data *data;
};

// Forward declaration
static void on_process(void *userdata, struct spa_io_position *position);

static const struct pw_filter_events filter_events = {
    PW_VERSION_FILTER_EVENTS,
    .process = on_process,
};

static void on_process(void *userdata, struct spa_io_position *position) {
    audio_backend_t *backend = (audio_backend_t *)userdata;
    pipewire_backend_data_t *data = (pipewire_backend_data_t *)backend->private_data;
    
    float *in = pw_filter_get_dsp_buffer(data->in_port, position->clock.duration);
    float *left_out = pw_filter_get_dsp_buffer(data->left_out_port, position->clock.duration);
    float *right_out = pw_filter_get_dsp_buffer(data->right_out_port, position->clock.duration);

    uint32_t n_samples = position->clock.duration;
    
    // Call the PWAR processing callback
    if (backend->callback) {
        backend->callback(in, left_out, right_out, n_samples, backend->userdata);
    } else {
        // Fallback: output silence if no callback
        if (left_out) memset(left_out, 0, n_samples * sizeof(float));
        if (right_out) memset(right_out, 0, n_samples * sizeof(float));
    }
}

static void *pipewire_thread_func(void *userdata) {
    audio_backend_t *backend = (audio_backend_t *)userdata;
    pipewire_backend_data_t *data = (pipewire_backend_data_t *)backend->private_data;
    
    printf("Starting PipeWire audio processing thread.\n");
    pw_main_loop_run(data->loop);
    printf("PipeWire audio processing thread stopped.\n");
    return NULL;
}

static int pipewire_init(audio_backend_t *backend, const audio_config_t *config,
                        audio_process_callback_t callback, void *userdata) {
    pipewire_backend_data_t *data = malloc(sizeof(pipewire_backend_data_t));
    if (!data) {
        return -1;
    }
    
    memset(data, 0, sizeof(pipewire_backend_data_t));
    data->config = *config;
    backend->private_data = data;
    backend->callback = callback;
    backend->userdata = userdata;
    
    // Set PipeWire latency environment variable
    char latency[32];
    snprintf(latency, sizeof(latency), "%d/%d", config->frames, config->sample_rate);
    setenv("PIPEWIRE_LATENCY", latency, 1);
    
    // Initialize PipeWire
    pw_init(NULL, NULL);
    data->loop = pw_main_loop_new(NULL);
    if (!data->loop) {
        free(data);
        return -1;
    }
    
    return 0;
}

static int pipewire_start(audio_backend_t *backend) {
    pipewire_backend_data_t *data = (pipewire_backend_data_t *)backend->private_data;
    if (!data || backend->running) {
        return -1;
    }
    
    // Create PipeWire filter
    const struct spa_pod *params[1];
    uint8_t buffer[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

    data->filter = pw_filter_new_simple(
        pw_main_loop_get_loop(data->loop),
        "pwar",
        pw_properties_new(
            PW_KEY_MEDIA_TYPE, "Audio",
            PW_KEY_MEDIA_CATEGORY, "Filter",
            PW_KEY_MEDIA_ROLE, "DSP",
            NULL),
        &filter_events,
        backend);  // Pass backend as userdata

    if (!data->filter) {
        return -1;
    }

    // Add input port
    data->in_port = pw_filter_add_port(data->filter,
        PW_DIRECTION_INPUT,
        PW_FILTER_PORT_FLAG_MAP_BUFFERS,
        sizeof(struct port),
        pw_properties_new(
            PW_KEY_FORMAT_DSP, "32 bit float mono audio",
            PW_KEY_PORT_NAME, "input",
            NULL),
        NULL, 0);

    // Add output ports
    data->left_out_port = pw_filter_add_port(data->filter,
        PW_DIRECTION_OUTPUT,
        PW_FILTER_PORT_FLAG_MAP_BUFFERS,
        sizeof(struct port),
        pw_properties_new(
            PW_KEY_FORMAT_DSP, "32 bit float mono audio",
            PW_KEY_PORT_NAME, "output-left",
            NULL),
        NULL, 0);

    data->right_out_port = pw_filter_add_port(data->filter,
        PW_DIRECTION_OUTPUT,
        PW_FILTER_PORT_FLAG_MAP_BUFFERS,
        sizeof(struct port),
        pw_properties_new(
            PW_KEY_FORMAT_DSP, "32 bit float mono audio",
            PW_KEY_PORT_NAME, "output-right",
            NULL),
        NULL, 0);

    // Set process latency parameter
    params[0] = spa_process_latency_build(&b,
        SPA_PARAM_ProcessLatency,
        &SPA_PROCESS_LATENCY_INFO_INIT(
            .ns = (uint64_t)data->config.frames * SPA_NSEC_PER_SEC / data->config.sample_rate
        ));

    // Connect the filter
    if (pw_filter_connect(data->filter,
            PW_FILTER_FLAG_RT_PROCESS,
            params, 1) < 0) {
        pw_filter_destroy(data->filter);
        data->filter = NULL;
        return -1;
    }

    // Start PipeWire main loop in separate thread
    data->should_stop = 0;
    if (pthread_create(&data->pw_thread, NULL, pipewire_thread_func, backend) != 0) {
        pw_filter_destroy(data->filter);
        data->filter = NULL;
        return -1;
    }
    
    backend->running = 1;
    printf("PipeWire backend started successfully.\n");
    return 0;
}

static int pipewire_stop(audio_backend_t *backend) {
    pipewire_backend_data_t *data = (pipewire_backend_data_t *)backend->private_data;
    if (!data || !backend->running) {
        return -1;
    }
    
    // Signal the PipeWire main loop to quit
    data->should_stop = 1;
    if (data->loop) {
        pw_main_loop_quit(data->loop);
    }
    
    // Wait for thread to finish
    pthread_join(data->pw_thread, NULL);
    
    // Destroy filter
    if (data->filter) {
        pw_filter_destroy(data->filter);
        data->filter = NULL;
    }
    
    backend->running = 0;
    printf("PipeWire backend stopped.\n");
    return 0;
}

static void pipewire_cleanup(audio_backend_t *backend) {
    pipewire_backend_data_t *data = (pipewire_backend_data_t *)backend->private_data;
    if (!data) {
        return;
    }
    
    if (backend->running) {
        pipewire_stop(backend);
    }
    
    // Cleanup PipeWire resources
    if (data->loop) {
        pw_main_loop_destroy(data->loop);
    }
    
    pw_deinit();
    
    free(data);
    backend->private_data = NULL;
    printf("PipeWire backend cleaned up.\n");
}

static int pipewire_is_running(audio_backend_t *backend) {
    return backend->running;
}

static void pipewire_get_stats(audio_backend_t *backend, void *stats) {
    // PipeWire doesn't provide the same level of statistics as ALSA
    // This could be extended to provide PipeWire-specific metrics
    (void)backend;
    (void)stats;
}

static float pipewire_get_latency(audio_backend_t *backend) {
    pipewire_backend_data_t *data = (pipewire_backend_data_t *)backend->private_data;
    if (!data || !data->config.sample_rate) return 0.0f;
    
    // Return latency based on the configured buffer size
    // PipeWire uses quantum (frames) for latency calculation
    return ((float)data->config.frames * 1000.0f) / (float)data->config.sample_rate;
}

static const audio_backend_ops_t pipewire_ops = {
    .init = pipewire_init,
    .start = pipewire_start,
    .stop = pipewire_stop,
    .cleanup = pipewire_cleanup,
    .is_running = pipewire_is_running,
    .get_stats = pipewire_get_stats,
    .get_latency = pipewire_get_latency
};

audio_backend_t* audio_backend_create_pipewire(void) {
    audio_backend_t *backend = malloc(sizeof(audio_backend_t));
    if (!backend) {
        return NULL;
    }
    
    memset(backend, 0, sizeof(audio_backend_t));
    backend->ops = &pipewire_ops;
    return backend;
}
