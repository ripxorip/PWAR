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
    uint64_t last_windows_recv;
    uint64_t last_linux_recv;

    float expected_interval_ms;
    uint32_t sample_rate;

    latency_stat_t rtt_stat;
    latency_stat_t audio_proc_stat;
    latency_stat_t windows_rcv_delta_stat;
    latency_stat_t linux_rcv_delta_stat;
    latency_stat_t ring_buffer_fill_level_stat;

    latency_stat_t rtt_stat_current;
    latency_stat_t audio_proc_stat_current;
    latency_stat_t windows_rcv_delta_stat_current;
    latency_stat_t linux_rcv_delta_stat_current;
    latency_stat_t ring_buffer_fill_level_stat_current;

    int64_t clock_offset_sum;
    uint32_t clock_offset_count;
    int64_t current_clock_offset;

    float audio_backend_latency_ms;

    uint64_t last_print_time;

} internal = {0};


static void process_latency_stat(latency_stat_t *stat, uint64_t value) {
    if (stat->count == 0 || value < stat->min) {
        stat->min = value;
    }
    if (stat->count == 0 || value > stat->max) {
        stat->max = value;
    }
    stat->total += value;
    stat->count++;
    stat->avg = stat->total / stat->count;
}


void latency_manager_init(uint32_t sample_rate, uint32_t buffer_size, float audio_backend_latency_ms) {
    internal.expected_interval_ms = (buffer_size / (float)sample_rate) * 1000.0f;
    internal.sample_rate = sample_rate;
    internal.audio_backend_latency_ms = audio_backend_latency_ms;
}


void latency_manager_report_ring_buffer_fill_level(uint32_t fill_level) {
    process_latency_stat(&internal.ring_buffer_fill_level_stat, fill_level);
}

void calculate_windows_clock_offset(pwar_packet_t *packet) {
   uint64_t estimated_rtt = internal.rtt_stat_current.avg;
    
    // Method 1: Linux->Windows direction
    // t2 should occur at approximately t1 + one_way_delay
    int64_t offset1 = ((int64_t)packet->t1_linux_send + (int64_t)(estimated_rtt / 2)) - (int64_t)packet->t2_windows_recv;
    
    // Method 2: Windows->Linux direction  
    // t3 should occur at approximately t4 - one_way_delay
    int64_t offset2 = ((int64_t)packet->t4_linux_recv - (int64_t)(estimated_rtt / 2)) - (int64_t)packet->t3_windows_send;
    
    // Average the two estimates for better accuracy
    int64_t combined_offset = (offset1 + offset2) / 2;
    
    internal.clock_offset_sum += combined_offset;
    internal.clock_offset_count++;
}

uint64_t convert_windows_to_linux_time(uint64_t windows_time) {
    return windows_time + internal.current_clock_offset;
}

void latency_manager_check_for_abnormalities(uint64_t rtt, pwar_packet_t *packet) {
    float rtt_ms = rtt / 1000000.0f;
    if (rtt_ms > internal.expected_interval_ms) {
        float linux_to_windows = (packet->t2_windows_recv - packet->t1_linux_send) / 1000000.0f;
        float windows_to_linux = (packet->t4_linux_recv - packet->t3_windows_send) / 1000000.0f;
        printf("\033[33m[PWAR][Warning]: High RTT detected: %.2fms (Linux->Windows: %.2fms, Windows->Linux: %.2fms)\033[0m\n", rtt_ms, linux_to_windows, windows_to_linux);
    }
}

void latency_manager_process_packet(pwar_packet_t *packet) {
    packet->t4_linux_recv = latency_manager_timestamp_now();
    calculate_windows_clock_offset(packet);

    uint64_t rtt = packet->t4_linux_recv - packet->t1_linux_send;
    uint64_t audio_proc = packet->t3_windows_send - packet->t2_windows_recv;

    uint64_t windows_rcv_delta = packet->t2_windows_recv - internal.last_windows_recv;
    uint64_t linux_rcv_delta = packet->t4_linux_recv - internal.last_linux_recv;

    process_latency_stat(&internal.rtt_stat, rtt);
    process_latency_stat(&internal.audio_proc_stat, audio_proc);
    process_latency_stat(&internal.windows_rcv_delta_stat, windows_rcv_delta);
    process_latency_stat(&internal.linux_rcv_delta_stat, linux_rcv_delta);

    internal.last_windows_recv = packet->t2_windows_recv;
    internal.last_linux_recv = packet->t4_linux_recv;

    // Convert the clocks
    packet->t2_windows_recv = convert_windows_to_linux_time(packet->t2_windows_recv);
    packet->t3_windows_send = convert_windows_to_linux_time(packet->t3_windows_send);
    latency_manager_check_for_abnormalities(rtt, packet);

    // Print stats every 2 seconds
    uint64_t current_time = latency_manager_timestamp_now();
    if (internal.last_print_time == 0) {
        internal.last_print_time = current_time;
    }
    
    uint64_t time_since_print = current_time - internal.last_print_time;
    if (time_since_print >= 2000000000ULL) { // 2 seconds in nanoseconds
        printf("[PWAR]: Audio RTT: min=%.2fms avg=%.2fms max=%.2fms | Net RTT: min=%.2fms avg=%.2fms max=%.2fms | AudioProc: min=%.2fms avg=%.2fms max=%.2fms | WinJitter: min=%.2fms avg=%.2fms max=%.2fms | LinuxJitter: min=%.2fms avg=%.2fms max=%.2fms\n",
               ((internal.ring_buffer_fill_level_stat.min / (float)internal.sample_rate) * 1000.0f) + internal.audio_backend_latency_ms,
               ((internal.ring_buffer_fill_level_stat.avg / (float)internal.sample_rate) * 1000.0f) + internal.audio_backend_latency_ms,
               ((internal.ring_buffer_fill_level_stat.max / (float)internal.sample_rate) * 1000.0f) + internal.audio_backend_latency_ms,
               internal.rtt_stat.min / 1000000.0,
               internal.rtt_stat.avg / 1000000.0,
               internal.rtt_stat.max / 1000000.0,
               internal.audio_proc_stat.min / 1000000.0,
               internal.audio_proc_stat.avg / 1000000.0,
               internal.audio_proc_stat.max / 1000000.0,
               internal.windows_rcv_delta_stat.min / 1000000.0,
               internal.windows_rcv_delta_stat.avg / 1000000.0,
               internal.windows_rcv_delta_stat.max / 1000000.0,
               internal.linux_rcv_delta_stat.min / 1000000.0,
               internal.linux_rcv_delta_stat.avg / 1000000.0,
               internal.linux_rcv_delta_stat.max / 1000000.0);

        internal.ring_buffer_fill_level_stat_current = internal.ring_buffer_fill_level_stat;
        internal.rtt_stat_current = internal.rtt_stat;
        internal.audio_proc_stat_current = internal.audio_proc_stat;
        internal.windows_rcv_delta_stat_current = internal.windows_rcv_delta_stat;
        internal.linux_rcv_delta_stat_current = internal.linux_rcv_delta_stat;
        
        // Reset stats for next period
        internal.rtt_stat = (latency_stat_t){0};
        internal.audio_proc_stat = (latency_stat_t){0};
        internal.windows_rcv_delta_stat = (latency_stat_t){0};
        internal.linux_rcv_delta_stat = (latency_stat_t){0};
        internal.ring_buffer_fill_level_stat = (latency_stat_t){0};

        // Update current clock offset estimate
        internal.current_clock_offset = internal.clock_offset_sum / (int64_t)internal.clock_offset_count;
        internal.clock_offset_sum = 0;
        internal.clock_offset_count = 0;

        internal.last_print_time = current_time;
    }
}

void latency_manger_get_current_metrics(pwar_latency_metrics_t *metrics) {
    metrics->rtt_min_ms = internal.rtt_stat_current.min / 1000000.0;
    metrics->rtt_max_ms = internal.rtt_stat_current.max / 1000000.0;
    metrics->rtt_avg_ms = internal.rtt_stat_current.avg / 1000000.0;
    metrics->audio_proc_min_ms = internal.audio_proc_stat_current.min / 1000000.0;
    metrics->audio_proc_max_ms = internal.audio_proc_stat_current.max / 1000000.0;
    metrics->audio_proc_avg_ms = internal.audio_proc_stat_current.avg / 1000000.0;
    metrics->windows_jitter_min_ms = internal.windows_rcv_delta_stat_current.min / 1000000.0;
    metrics->windows_jitter_max_ms = internal.windows_rcv_delta_stat_current.max / 1000000.0;
    metrics->windows_jitter_avg_ms = internal.windows_rcv_delta_stat_current.avg / 1000000.0;   
    metrics->linux_jitter_min_ms = internal.linux_rcv_delta_stat_current.min / 1000000.0;
    metrics->linux_jitter_max_ms = internal.linux_rcv_delta_stat_current.max / 1000000.0;
    metrics->linux_jitter_avg_ms = internal.linux_rcv_delta_stat_current.avg / 1000000.0;   
    metrics->ring_buffer_min_ms = (internal.ring_buffer_fill_level_stat_current.min / (float)internal.sample_rate) * 1000.0f;
    metrics->ring_buffer_max_ms = (internal.ring_buffer_fill_level_stat_current.max / (float)internal.sample_rate) * 1000.0f;
    metrics->ring_buffer_avg_ms = (internal.ring_buffer_fill_level_stat_current.avg / (float)internal.sample_rate) * 1000.0f;
    metrics->xruns = 0; // XRUNs tracking not implemented yet
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

