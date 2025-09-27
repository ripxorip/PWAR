#ifndef PWAR_LATENCY_TYPES
#define PWAR_LATENCY_TYPES

typedef struct {
    // Round-trip time
    double rtt_min_ms;
    double rtt_max_ms;
    double rtt_avg_ms;
    
    // Audio processing time
    double audio_proc_min_ms;
    double audio_proc_max_ms;
    double audio_proc_avg_ms;
    
    // Windows receive delta (jitter)
    double windows_jitter_min_ms;
    double windows_jitter_max_ms;
    double windows_jitter_avg_ms;
    
    // Linux receive delta (jitter)
    double linux_jitter_min_ms;
    double linux_jitter_max_ms;
    double linux_jitter_avg_ms;

    // Ring buffer fill level (in milliseconds)
    double ring_buffer_min_ms;
    double ring_buffer_max_ms;
    double ring_buffer_avg_ms;

    uint32_t xruns;
} pwar_latency_metrics_t;

#endif /* PWAR_LATENCY_TYPES */
