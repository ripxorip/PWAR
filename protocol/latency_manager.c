#include "latency_manager.h"

#ifdef __linux__
#include <time.h>
#include <stdio.h>
#elif defined(_WIN32)
#include <windows.h>
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
    //uint64_t seq = packet->seq; // Sequence number of the packet

    // From Windows (num_packets will always be 1 in this case)
    uint64_t packet_ts = packet->timestamp; // Timestamp from the packet when it was sent

    uint64_t nowNs = latency_manager_timestamp_now();

    // Timestamps from the local system
    uint64_t time_since_last_local_packet = nowNs - internal.last_local_packet_timestamp;
    internal.last_local_packet_timestamp = nowNs;

    // Timestamps from pipewire (i.e. between each chunk)
    uint64_t audio_ckb_interval = packet_ts - internal.last_remote_packet_timestamp;
    internal.last_remote_packet_timestamp = packet_ts;

    // With low jitter they should be similar, but with high jitter they can differ significantly
    uint64_t jitter = time_since_last_local_packet - audio_ckb_interval;
    printf("Audio callback interval: %llu ns\n", audio_ckb_interval);
    printf("Time since last local packet: %llu ns\n", time_since_last_local_packet);
    internal.network_jitter.total += jitter;
    internal.network_jitter.count++;
    if (jitter < internal.network_jitter.min || internal.network_jitter.count == 1) {
        internal.network_jitter.min = jitter;
    }
    if (jitter > internal.network_jitter.max || internal.network_jitter.count == 1) {
        internal.network_jitter.max = jitter;
    }
}

void latency_manager_process_packet_server(pwar_packet_t *packet) {
    // From Linux (num_packets can be larger than 1 due to windows buffering)
    uint64_t packet_ts = packet->timestamp; // Timestamp from the packet when it was sent
    uint64_t seq = packet->seq; // Sequence number of the packet

    uint64_t nowNs = latency_manager_timestamp_now();

    // Timestamps from the local system
    uint64_t time_since_last_local_packet = nowNs - internal.last_local_packet_timestamp;
    internal.last_local_packet_timestamp = nowNs;

    // Timestamps from Windows
    uint64_t time_since_last_remote_packet = packet_ts - internal.last_remote_packet_timestamp;
    internal.last_remote_packet_timestamp = packet_ts;


    if (seq != internal.current_seq) {
        // New sequence detected, reset state
        internal.current_seq = seq;
    }
    else {
        // Same sequence, this is from a burst of packets
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
    // Print all stats as ms
    printf("Audio Processing: Min: %.3f ms, Max: %.3f ms, Avg: %.3f ms\n",
           latency_info->audio_proc_min / 1000000.0,
           latency_info->audio_proc_max / 1000000.0,
           latency_info->audio_proc_avg / 1000000.0);
    printf("Network Jitter: Min: %.3f ms, Max: %.3f ms, Avg: %.3f ms\n",
           latency_info->jitter_min / 1000000.0,
           latency_info->jitter_max / 1000000.0,
           latency_info->jitter_avg / 1000000.0);
    printf("-- Latency info handled --\n");
}

uint64_t latency_manager_timestamp_now() {
#ifdef __linux__
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000 + ts.tv_nsec;

#elif defined(_WIN32)
    uint64_t nowNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    return nowNs;
#endif
}

