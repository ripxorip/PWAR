#ifndef AUDIO_BACKEND
#define AUDIO_BACKEND

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Audio backend types
typedef enum {
    AUDIO_BACKEND_ALSA,
    AUDIO_BACKEND_PIPEWIRE,
    AUDIO_BACKEND_SIMULATED     // Always available for testing
} audio_backend_type_t;

// Audio configuration
typedef struct {
    const char *device_playback;    // For ALSA: "hw:3,0", for PipeWire: NULL
    const char *device_capture;     // For ALSA: "hw:3,0", for PipeWire: NULL
    uint32_t sample_rate;
    uint32_t frames;                // Buffer size in frames
    uint32_t playback_channels;
    uint32_t capture_channels;
} audio_config_t;

// Forward declaration of audio backend instance
struct audio_backend;

// Audio processing callback function type
// Called by audio backend when new audio data is available
// in: input audio samples (interleaved)
// out_left, out_right: output audio samples (non-interleaved)
// n_samples: number of samples per channel
// userdata: user data passed to backend initialization
typedef void (*audio_process_callback_t)(float *in, float *out_left, float *out_right, 
                                        uint32_t n_samples, void *userdata);

// Audio backend interface
typedef struct audio_backend_ops {
    // Initialize the audio backend
    int (*init)(struct audio_backend *backend, const audio_config_t *config, 
                audio_process_callback_t callback, void *userdata);
    
    // Start audio processing
    int (*start)(struct audio_backend *backend);
    
    // Stop audio processing
    int (*stop)(struct audio_backend *backend);
    
    // Cleanup resources
    void (*cleanup)(struct audio_backend *backend);
    
    // Check if backend is running
    int (*is_running)(struct audio_backend *backend);
    
    // Get backend-specific statistics (optional)
    void (*get_stats)(struct audio_backend *backend, void *stats);

    // Get current backend latency in milliseconds
    float (*get_latency)(struct audio_backend *backend);
} audio_backend_ops_t;

// Audio backend instance
typedef struct audio_backend {
    const audio_backend_ops_t *ops;
    void *private_data;
    audio_process_callback_t callback;
    void *userdata;
    int running;
} audio_backend_t;

// Factory functions for creating backends (conditionally available)
#ifdef HAVE_ALSA
audio_backend_t* audio_backend_create_alsa(void);
#endif

#ifdef HAVE_PIPEWIRE
audio_backend_t* audio_backend_create_pipewire(void);
#endif

// Simulated backend (always available for testing)
audio_backend_t* audio_backend_create_simulated(void);

// Unified factory function (always available)
audio_backend_t* audio_backend_create(audio_backend_type_t type);

// Check if a backend type is available at runtime
int audio_backend_is_available(audio_backend_type_t type);

// Generic backend operations
int audio_backend_init(audio_backend_t *backend, const audio_config_t *config,
                      audio_process_callback_t callback, void *userdata);
int audio_backend_start(audio_backend_t *backend);
int audio_backend_stop(audio_backend_t *backend);
void audio_backend_cleanup(audio_backend_t *backend);
int audio_backend_is_running(audio_backend_t *backend);
void audio_backend_get_stats(audio_backend_t *backend, void *stats);

// Get current backend latency in milliseconds
float audio_backend_get_latency(audio_backend_t *backend);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_BACKEND */
