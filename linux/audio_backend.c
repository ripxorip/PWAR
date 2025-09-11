// Generic Audio Backend Implementation
// Provides common interface for all audio backends

#include "audio_backend.h"
#include <stdlib.h>

// Generic backend operations
int audio_backend_init(audio_backend_t *backend, const audio_config_t *config,
                      audio_process_callback_t callback, void *userdata) {
    if (!backend || !backend->ops || !backend->ops->init) {
        return -1;
    }
    return backend->ops->init(backend, config, callback, userdata);
}

int audio_backend_start(audio_backend_t *backend) {
    if (!backend || !backend->ops || !backend->ops->start) {
        return -1;
    }
    return backend->ops->start(backend);
}

int audio_backend_stop(audio_backend_t *backend) {
    if (!backend || !backend->ops || !backend->ops->stop) {
        return -1;
    }
    return backend->ops->stop(backend);
}

void audio_backend_cleanup(audio_backend_t *backend) {
    if (!backend || !backend->ops || !backend->ops->cleanup) {
        return;
    }
    backend->ops->cleanup(backend);
    free(backend);
}

int audio_backend_is_running(audio_backend_t *backend) {
    if (!backend || !backend->ops || !backend->ops->is_running) {
        return 0;
    }
    return backend->ops->is_running(backend);
}

void audio_backend_get_stats(audio_backend_t *backend, void *stats) {
    if (!backend || !backend->ops || !backend->ops->get_stats) {
        return;
    }
    backend->ops->get_stats(backend, stats);
}

float audio_backend_get_latency(audio_backend_t *backend) {
    if (!backend || !backend->ops || !backend->ops->get_latency) {
        return 0.0f;
    }
    return backend->ops->get_latency(backend);
}

// Unified factory function
audio_backend_t* audio_backend_create(audio_backend_type_t type) {
    switch (type) {
#ifdef HAVE_ALSA
        case AUDIO_BACKEND_ALSA:
            return audio_backend_create_alsa();
#endif
#ifdef HAVE_PIPEWIRE
        case AUDIO_BACKEND_PIPEWIRE:
            return audio_backend_create_pipewire();
#endif
        case AUDIO_BACKEND_SIMULATED:
            return audio_backend_create_simulated();
        default:
            return NULL;
    }
}

// Check if a backend type is available at runtime
int audio_backend_is_available(audio_backend_type_t type) {
    switch (type) {
#ifdef HAVE_ALSA
        case AUDIO_BACKEND_ALSA:
            return 1;
#endif
#ifdef HAVE_PIPEWIRE
        case AUDIO_BACKEND_PIPEWIRE:
            return 1;
#endif
        case AUDIO_BACKEND_SIMULATED:
            return 1;  // Always available
        default:
            return 0;
    }
}
