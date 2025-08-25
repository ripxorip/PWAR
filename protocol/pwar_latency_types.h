#ifndef PWAR_LATENCY_TYPES
#define PWAR_LATENCY_TYPES

typedef struct {
    double audio_proc_min_ms;
    double audio_proc_max_ms;
    double audio_proc_avg_ms;
    double jitter_min_ms;
    double jitter_max_ms;
    double jitter_avg_ms;
    double rtt_min_ms;
    double rtt_max_ms;
    double rtt_avg_ms;

    uint32_t xruns;
} pwar_latency_metrics_t;

#endif /* PWAR_LATENCY_TYPES */
