#include "latency_manager.h"
#include <stdio.h>

#ifdef __linux__
#include <time.h>
#elif defined(_WIN32)
#include <windows.h>
#define WIN32_LEAN_AND_MEAN
#endif

typedef struct {
    uint64_t min;
    uint64_t max;
    uint64_t avg;
    uint64_t total;
    uint64_t count;
} latency_stat_t;

static struct {
    uint64_t last_latency_info_sent; // Timestamp of the last latency info sent

    uint64_t last_local_packet_timestamp; // Timestamp of the last packet processed
    uint64_t last_remote_packet_timestamp; // Timestamp of the last remote packet processed

    // ----
    uint64_t current_seq; // Current sequence number for packets
    uint32_t current_index;
    uint32_t num_packets; // Total number of packets in the current sequence

    uint64_t audio_ckb_start_timestamp; // Timestamp when the audio callback started
    uint64_t audio_ckb_end_timestamp; // Timestamp when the audio callback ended

    // ----
    latency_stat_t audio_proc; // Statistics for audio processing
    latency_stat_t network_jitter; // Statistics for network jitter
    latency_stat_t round_trip_time; // Statistics for round trip time

    uint32_t xruns_2sec; // Number of xruns in the last 2 seconds
    uint32_t xruns;

} internal = {0};

void latency_manager_init() {

}

void latency_manager_start_audio_cbk_begin() {
    internal.audio_ckb_start_timestamp = latency_manager_timestamp_now();
}

void latency_manager_start_audio_cbk_end() {
    internal.audio_ckb_end_timestamp = latency_manager_timestamp_now();
    // Calculate the duration of the audio callback
    uint64_t duration = internal.audio_ckb_end_timestamp - internal.audio_ckb_start_timestamp;
    internal.audio_proc.total += duration;
    internal.audio_proc.count++;
    if (duration < internal.audio_proc.min || internal.audio_proc.count == 1) {
        internal.audio_proc.min = duration;
    }
    if (duration > internal.audio_proc.max || internal.audio_proc.count == 1) {
        internal.audio_proc.max = duration;
    }
}

void latency_manager_process_packet_client(pwar_packet_t *packet) {
    uint64_t packet_ts = packet->timestamp;
    uint64_t nowNs = latency_manager_timestamp_now();

    uint64_t time_since_last_local_packet = nowNs - internal.last_local_packet_timestamp;
    internal.last_local_packet_timestamp = nowNs;

    uint64_t audio_ckb_interval = packet_ts - internal.last_remote_packet_timestamp;
    internal.last_remote_packet_timestamp = packet_ts;

    // Jitter can be negative, log signed value
    int64_t jitter = (int64_t)time_since_last_local_packet - (int64_t)audio_ckb_interval;
    uint64_t abs_jitter = (jitter < 0) ? -jitter : jitter;
    internal.network_jitter.total += abs_jitter;
    internal.network_jitter.count++;
    if (abs_jitter < internal.network_jitter.min || internal.network_jitter.count == 1) {
        internal.network_jitter.min = abs_jitter;
    }
    if (abs_jitter > internal.network_jitter.max || internal.network_jitter.count == 1) {
        internal.network_jitter.max = abs_jitter;
    }
}

void latency_manager_process_packet_server(pwar_packet_t *packet) {
    if (packet->packet_index == packet->num_packets - 1) {
        uint64_t round_trip_time = latency_manager_timestamp_now() - packet->seq_timestamp;
        internal.round_trip_time.total += round_trip_time;
        internal.round_trip_time.count++;
        if (round_trip_time < internal.round_trip_time.min || internal.round_trip_time.count == 1) {
            internal.round_trip_time.min = round_trip_time;
        }
        if (round_trip_time > internal.round_trip_time.max || internal.round_trip_time.count == 1) {
            internal.round_trip_time.max = round_trip_time;
        }
    }
}


int latency_manager_time_for_sending_latency_info(pwar_latency_info_t *latency_info) {
    // Send latency info every 2 seconds
    uint64_t now = latency_manager_timestamp_now();
    if (now - internal.last_latency_info_sent >= 2 * 1000000000) {
        internal.audio_proc.avg = (internal.audio_proc.count > 0) ? (internal.audio_proc.total / internal.audio_proc.count) : 0;
        internal.network_jitter.avg = (internal.network_jitter.count > 0) ? (internal.network_jitter.total / internal.network_jitter.count) : 0;

        // Fill in audio_proc stats
        latency_info->audio_proc_avg = internal.audio_proc.avg;
        latency_info->audio_proc_min = internal.audio_proc.min;
        latency_info->audio_proc_max = internal.audio_proc.max;

        // Reset audio_proc
        internal.audio_proc.min = UINT64_MAX;
        internal.audio_proc.max = 0;
        internal.audio_proc.total = 0;
        internal.audio_proc.count = 0;


        // Fill in network jitter stats
        latency_info->jitter_avg = internal.network_jitter.avg;
        latency_info->jitter_min = internal.network_jitter.min;
        latency_info->jitter_max = internal.network_jitter.max;

        // Reset network jitter
        internal.network_jitter.min = UINT64_MAX;
        internal.network_jitter.max = 0;
        internal.network_jitter.total = 0;
        internal.network_jitter.count = 0;

        internal.last_latency_info_sent = now;
        return 1; // Indicate that latency info should be sent
    }
    return 0;
}

void latency_manager_handle_latency_info(pwar_latency_info_t *latency_info) {
    // Print all stats as ms in one streamlined line
    internal.round_trip_time.avg = (internal.round_trip_time.count > 0) ? (internal.round_trip_time.total / internal.round_trip_time.count) : 0;
    printf("[PWAR]: AudioProc: min=%.3fms max=%.3fms avg=%.3fms | Jitter: min=%.3fms max=%.3fms avg=%.3fms | RTT: min=%.3fms max=%.3fms avg=%.3fms\n",
        latency_info->audio_proc_min / 1000000.0,
        latency_info->audio_proc_max / 1000000.0,
        latency_info->audio_proc_avg / 1000000.0,
        latency_info->jitter_min / 1000000.0,
        latency_info->jitter_max / 1000000.0,
        latency_info->jitter_avg / 1000000.0,
        internal.round_trip_time.min / 1000000.0,
        internal.round_trip_time.max / 1000000.0,
        internal.round_trip_time.avg / 1000000.0);

    internal.round_trip_time.min = UINT64_MAX;
    internal.round_trip_time.max = 0;
    internal.round_trip_time.total = 0;
    internal.round_trip_time.count = 0;

    internal.audio_proc.min = latency_info->audio_proc_min;
    internal.audio_proc.max = latency_info->audio_proc_max;
    internal.audio_proc.avg = latency_info->audio_proc_avg;

    internal.network_jitter.min = latency_info->jitter_min;
    internal.network_jitter.max = latency_info->jitter_max;
    internal.network_jitter.avg = latency_info->jitter_avg;

    internal.xruns_2sec = internal.xruns; // Store xruns for the last 2 seconds
    internal.xruns = 0; // Reset xruns count
}

void latency_manager_get_current_metrics(pwar_latency_metrics_t *metrics) {
    if (!metrics) return;
    
    // Calculate averages for current data
    internal.audio_proc.avg = (internal.audio_proc.count > 0) ? (internal.audio_proc.total / internal.audio_proc.count) : 0;
    internal.network_jitter.avg = (internal.network_jitter.count > 0) ? (internal.network_jitter.total / internal.network_jitter.count) : 0;
    internal.round_trip_time.avg = (internal.round_trip_time.count > 0) ? (internal.round_trip_time.total / internal.round_trip_time.count) : 0;
    
    // Convert from nanoseconds to milliseconds
    metrics->audio_proc_min_ms = internal.audio_proc.min / 1000000.0;
    metrics->audio_proc_max_ms = internal.audio_proc.max / 1000000.0;
    metrics->audio_proc_avg_ms = internal.audio_proc.avg / 1000000.0;
    
    metrics->jitter_min_ms = internal.network_jitter.min / 1000000.0;
    metrics->jitter_max_ms = internal.network_jitter.max / 1000000.0;
    metrics->jitter_avg_ms = internal.network_jitter.avg / 1000000.0;
    
    metrics->rtt_min_ms = internal.round_trip_time.min / 1000000.0;
    metrics->rtt_max_ms = internal.round_trip_time.max / 1000000.0;
    metrics->rtt_avg_ms = internal.round_trip_time.avg / 1000000.0;

    if (internal.xruns_2sec == 0) {
        if (internal.xruns > 1000) internal.xruns = 1000;
        metrics->xruns = internal.xruns; // No xruns in the last 2 seconds
    } else {
        metrics->xruns = internal.xruns_2sec; // Return the xruns count from the last 2 seconds
    }
}


void latency_manager_report_xrun() {
    internal.xruns++;
}

uint64_t latency_manager_timestamp_now() {
#ifdef __linux__
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000 + ts.tv_nsec;
#elif defined(_WIN32)
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (uint64_t)((counter.QuadPart * 1000000000ULL) / freq.QuadPart);
#endif
}

