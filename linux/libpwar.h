#ifndef LIBPWAR
#define LIBPWAR

#include <stdint.h>
#include "../protocol/pwar_latency_types.h"
#include "audio_backend.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PWAR_MAX_IP_LEN 64

typedef struct {
    char stream_ip[PWAR_MAX_IP_LEN];
    int stream_port;
    int passthrough_test;
    int device_buffer_size;     // Audio device buffer size in frames (32/64/128 etc.)
    int windows_packet_size;    // Windows packet buffer size in frames (64/128 etc.)
    int ring_buffer_depth;      // Depth of the ring buffer for audio processing
    audio_backend_type_t backend_type;
    audio_config_t audio_config;
} pwar_config_t;

int pwar_cli_run(const pwar_config_t *config);

// New GUI functions
int pwar_requires_restart(const pwar_config_t *old_config, const pwar_config_t *new_config);
int pwar_update_config(const pwar_config_t *config);
int pwar_init(const pwar_config_t *config);
int pwar_start(void);
int pwar_stop(void);
void pwar_cleanup(void);
int pwar_is_running(void);

// Get current latency metrics
void pwar_get_latency_metrics(pwar_latency_metrics_t *metrics);

// Get current Windows buffer size in samples
uint32_t pwar_get_current_windows_buffer_size(void);

#ifdef __cplusplus
}
#endif

#endif /* LIBPWAR */
