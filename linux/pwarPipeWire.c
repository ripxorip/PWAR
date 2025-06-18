/*
 * pwarPipeWire.c - PipeWire <-> UDP streaming bridge for PWAR
 *
 * (c) 2025 Philip K. Gisslow
 * This file is part of the PipeWire ASIO Relay (PWAR) project.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <unistd.h>
#include <spa/pod/builder.h>
#include <spa/param/latency-utils.h>
#include <pipewire/pipewire.h>
#include <pipewire/filter.h>
#include "pwar_packet.h"

#define DEFAULT_STREAM_IP "192.168.66.3"
#define DEFAULT_MIDI_STREAM_IP "10.0.0.184"
#define DEFAULT_STREAM_PORT 8321

struct data;

struct port {
    struct data *data;
};

struct data {
    struct pw_main_loop *loop;
    struct pw_filter *filter;
    struct port *in_port;
    struct port *left_out_port;
    struct port *right_out_port;
    struct port *midi_in_port;
    float sine_phase;
    uint8_t test_mode;
    uint32_t seq;
    int sockfd;
    struct sockaddr_in servaddr;
    int recv_sockfd;

    int midi_sockfd;
    struct sockaddr_in midi_servaddr;

    pthread_mutex_t packet_mutex;
    pthread_cond_t packet_cond;
    rt_stream_packet_t latest_packet;
    int packet_available;
};

static void setup_recv_socket(struct data *data, int port);
static void *receiver_thread(void *userdata);

static void setup_socket(struct data *data, const char *ip, int port);
static void setup_midi_socket(struct data *data, const char *ip, int port);

static void stream_buffer(float *samples, uint32_t n_samples, void *userdata);
static void on_process(void *userdata, struct spa_io_position *position);
static void do_quit(void *userdata, int signal_number);

static void setup_recv_socket(struct data *data, int port) {
    data->recv_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (data->recv_sockfd < 0) {
        perror("recv socket creation failed");
        exit(EXIT_FAILURE);
    }
    // Increase UDP receive buffer to 1MB to reduce risk of overrun
    int rcvbuf = 1024 * 1024;
    if (setsockopt(data->recv_sockfd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf)) < 0) {
        perror("setsockopt SO_RCVBUF failed");
    }
    struct sockaddr_in recv_addr;
    memset(&recv_addr, 0, sizeof(recv_addr));
    recv_addr.sin_family = AF_INET;
    recv_addr.sin_addr.s_addr = INADDR_ANY;
    recv_addr.sin_port = htons(port);
    if (bind(data->recv_sockfd, (struct sockaddr *)&recv_addr, sizeof(recv_addr)) < 0) {
        perror("recv socket bind failed");
        exit(EXIT_FAILURE);
    }
}

static void *receiver_thread(void *userdata) {
    // Set real-time scheduling to minimize jitter
    struct sched_param sp = { .sched_priority = 90 };
    if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp) != 0) {
        perror("Warning: Failed to set SCHED_FIFO for receiver_thread");
    }

    struct data *data = (struct data *)userdata;
    rt_stream_packet_t packet;
    // Latency stats
    static double min_total = 1e9, max_total = 0, sum_total = 0;
    static double min_daw = 1e9, max_daw = 0, sum_daw = 0;
    static double min_net = 1e9, max_net = 0, sum_net = 0;
    static uint64_t last_print_ns = 0;
    static int count = 0;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    last_print_ns = (uint64_t)ts.tv_sec * 1000000000 + ts.tv_nsec;

    while (1) {
        ssize_t n = recvfrom(data->recv_sockfd, &packet, sizeof(packet), 0, NULL, NULL);
        if (n == (ssize_t)sizeof(packet)) {
            pthread_mutex_lock(&data->packet_mutex);
            data->latest_packet = packet;
            data->packet_available = 1;
            pthread_cond_signal(&data->packet_cond);
            pthread_mutex_unlock(&data->packet_mutex);

            clock_gettime(CLOCK_MONOTONIC, &ts);
            uint64_t ts_return = (uint64_t)ts.tv_sec * 1000000000 + ts.tv_nsec;
            uint64_t total_latency = ts_return - packet.ts_pipewire_send;
            uint64_t daw_latency = packet.ts_asio_send - packet.ts_pipewire_send;
            uint64_t network_latency = total_latency - daw_latency;
            double total_latency_ms = total_latency / 1000000.0;
            double daw_latency_ms = daw_latency / 1000000.0;
            double network_latency_ms = network_latency / 1000000.0;

            // Update stats
            if (total_latency_ms < min_total) min_total = total_latency_ms;
            if (total_latency_ms > max_total) max_total = total_latency_ms;
            sum_total += total_latency_ms;

            if (daw_latency_ms < min_daw) min_daw = daw_latency_ms;
            if (daw_latency_ms > max_daw) max_daw = daw_latency_ms;
            sum_daw += daw_latency_ms;

            if (network_latency_ms < min_net) min_net = network_latency_ms;
            if (network_latency_ms > max_net) max_net = network_latency_ms;
            sum_net += network_latency_ms;

            count++;

            // Print every 2 seconds
            uint64_t now_ns = ts_return;
            if (now_ns - last_print_ns >= 2 * 1000000000ULL) {
                double avg_total = count ? sum_total / count : 0;
                double avg_daw = count ? sum_daw / count : 0;
                double avg_net = count ? sum_net / count : 0;
                printf("[2s] Packets: %d | Total Latency: min %.2f ms, max %.2f ms, avg %.2f ms | DAW: min %.2f ms, max %.2f ms, avg %.2f ms | Net: min %.2f ms, max %.2f ms, avg %.2f ms\n",
                    count, min_total, max_total, avg_total, min_daw, max_daw, avg_daw, min_net, max_net, avg_net);
                // Reset stats
                min_total = min_daw = min_net = 1e9;
                max_total = max_daw = max_net = 0;
                sum_total = sum_daw = sum_net = 0;
                count = 0;
                last_print_ns = now_ns;
            }
        }
    }
    return NULL;
}

static void setup_midi_socket(struct data *data, const char *ip, int port) {
    data->midi_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (data->midi_sockfd < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }
    memset(&data->midi_servaddr, 0, sizeof(data->midi_servaddr));
    data->midi_servaddr.sin_family = AF_INET;
    data->midi_servaddr.sin_port = htons(port);
    data->midi_servaddr.sin_addr.s_addr = inet_addr(ip);
}

static void setup_socket(struct data *data, const char *ip, int port) {
    data->sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (data->sockfd < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }
    memset(&data->servaddr, 0, sizeof(data->servaddr));
    data->servaddr.sin_family = AF_INET;
    data->servaddr.sin_port = htons(port);
    data->servaddr.sin_addr.s_addr = inet_addr(ip);
}

static void stream_buffer(float *samples, uint32_t n_samples, void *userdata) {
    struct data *data = (struct data *)userdata;
    rt_stream_packet_t packet;
    packet.seq = data->seq++;
    packet.n_samples = n_samples;
    memcpy(packet.samples_ch1, samples, n_samples * sizeof(float));
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t timestamp = (uint64_t)ts.tv_sec * 1000000000 + ts.tv_nsec;
    packet.ts_pipewire_send = timestamp;
    if (sendto(data->sockfd, &packet, sizeof(packet), 0, (struct sockaddr *)&data->servaddr, sizeof(data->servaddr)) < 0) {
        perror("sendto failed");
    }
}

static void on_process(void *userdata, struct spa_io_position *position) {
    struct data *data = (struct data *)userdata;
    float *in = pw_filter_get_dsp_buffer(data->in_port, position->clock.duration);
    float *left_out = pw_filter_get_dsp_buffer(data->left_out_port, position->clock.duration);
    float *right_out = pw_filter_get_dsp_buffer(data->right_out_port, position->clock.duration);
    struct pw_buffer *midi_buf = pw_filter_dequeue_buffer(data->midi_in_port);

    // FIXME: This is a hack to get the MIDI data from the first buffer.
    // In a real application, you would want to handle multiple buffers and MIDI events properly. (Good first issue)
    if (midi_buf && midi_buf->buffer && midi_buf->buffer->datas[0].data && midi_buf->buffer->datas[0].chunk) {
        uint8_t midi_bytes[3];
        uint8_t *midi_data = midi_buf->buffer->datas[0].data;
        uint32_t midi_size = midi_buf->buffer->datas[0].chunk->size;

        if (midi_size == 40) {
            midi_bytes[0] = midi_data[32];
            midi_bytes[1] = midi_data[33];
            midi_bytes[2] = midi_data[34];
            printf("Sending MIDI data: %02x %02x %02x\n", midi_bytes[0], midi_bytes[1], midi_bytes[2]);
            // Ready to send MIDI data
            if (sendto(data->midi_sockfd, midi_bytes, sizeof(midi_bytes), 0,
                       (struct sockaddr *)&data->midi_servaddr, sizeof(data->midi_servaddr)) < 0) {
                perror("sendto MIDI failed");
            }
        }
        pw_filter_queue_buffer(data->midi_in_port, midi_buf);
    }

    uint32_t n_samples = position->clock.duration;
    if (data->test_mode) {
        for (uint32_t n = 0; n < n_samples; n++) {
            if (data->sine_phase >= 2 * M_PI)
                data->sine_phase -= 2 * M_PI;
            in[n] = sinf(data->sine_phase) * 0.5f;
            data->sine_phase += 2 * M_PI * 440 / 48000;
        }
    }
    stream_buffer(in, n_samples, data);
    int got_packet = 0;
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_nsec += 2 * 1000 * 1000;
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec += 1;
        ts.tv_nsec -= 1000000000;
    }
    pthread_mutex_lock(&data->packet_mutex);
    while (!data->packet_available) {
        int rc = pthread_cond_timedwait(&data->packet_cond, &data->packet_mutex, &ts);
        if (rc == ETIMEDOUT)
            break;
    }
    if (data->packet_available) {
        if (left_out)
            memcpy(left_out, data->latest_packet.samples_ch1, n_samples * sizeof(float));
        if (right_out)
            memcpy(right_out, data->latest_packet.samples_ch2, n_samples * sizeof(float));
        got_packet = 1;
        data->packet_available = 0;
    }
    pthread_mutex_unlock(&data->packet_mutex);
    if (!got_packet) {
        fprintf(stderr, "--- ERROR -- No valid packet received, outputting silence\n");
        fprintf(stderr, "I wanted seq: %u and got seq: %lu\n", data->seq - 1, data->latest_packet.seq);
        if (left_out)
            memset(left_out, 0, n_samples * sizeof(float));
        if (right_out)
            memset(right_out, 0, n_samples * sizeof(float));
    }
}

static const struct pw_filter_events filter_events = {
    PW_VERSION_FILTER_EVENTS,
    .process = on_process,
};

static void do_quit(void *userdata, int signal_number) {
    struct data *data = (struct data *)userdata;
    pw_main_loop_quit(data->loop);
}

int main(int argc, char *argv[]) {
    char stream_ip[64] = DEFAULT_STREAM_IP;
    char midi_stream_ip[64] = DEFAULT_MIDI_STREAM_IP;
    int stream_port = DEFAULT_STREAM_PORT;
    int test_mode = 0;
    for (int i = 1; i < argc; ++i) {
        if ((strcmp(argv[i], "--ip") == 0 || strcmp(argv[i], "-i") == 0) && i + 1 < argc) {
            strncpy(stream_ip, argv[++i], sizeof(stream_ip) - 1);
            stream_ip[sizeof(stream_ip) - 1] = '\0';
        } else if ((strcmp(argv[i], "--port") == 0 || strcmp(argv[i], "-p") == 0) && i + 1 < argc) {
            stream_port = atoi(argv[++i]);
        } else if ((strcmp(argv[i], "--test") == 0) || (strcmp(argv[i], "-t") == 0)) {
            test_mode = 1;
        }
    }
    char latency[32];
    snprintf(latency, sizeof(latency), "%d/48000", 128);
    setenv("PIPEWIRE_LATENCY", latency, 1);
    struct data data;
    memset(&data, 0, sizeof(data));
    const struct spa_pod *params[1];
    uint8_t buffer[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

    setup_socket(&data, stream_ip, stream_port);
    setup_midi_socket(&data, midi_stream_ip, stream_port);

    setup_recv_socket(&data, stream_port);
    pthread_mutex_init(&data.packet_mutex, NULL);
    pthread_cond_init(&data.packet_cond, NULL);
    data.packet_available = 0;
    pthread_t recv_thread;
    pthread_create(&recv_thread, NULL, receiver_thread, &data);
    pw_init(&argc, &argv);
    data.loop = pw_main_loop_new(NULL);
    pw_loop_add_signal(pw_main_loop_get_loop(data.loop), SIGINT, do_quit, &data);
    pw_loop_add_signal(pw_main_loop_get_loop(data.loop), SIGTERM, do_quit, &data);
    data.test_mode = test_mode;
    data.sine_phase = 0.0f;
    data.filter = pw_filter_new_simple(
        pw_main_loop_get_loop(data.loop),
        "pwar",
        pw_properties_new(
            PW_KEY_MEDIA_TYPE, "Audio",
            PW_KEY_MEDIA_CATEGORY, "Filter",
            PW_KEY_MEDIA_ROLE, "DSP",
            NULL),
        &filter_events,
        &data);
    data.in_port = pw_filter_add_port(data.filter,
        PW_DIRECTION_INPUT,
        PW_FILTER_PORT_FLAG_MAP_BUFFERS,
        sizeof(struct port),
        pw_properties_new(
            PW_KEY_FORMAT_DSP, "32 bit float mono audio",
            PW_KEY_PORT_NAME, "input",
            NULL),
        NULL, 0);
    data.left_out_port = pw_filter_add_port(data.filter,
        PW_DIRECTION_OUTPUT,
        PW_FILTER_PORT_FLAG_MAP_BUFFERS,
        sizeof(struct port),
        pw_properties_new(
            PW_KEY_FORMAT_DSP, "32 bit float mono audio",
            PW_KEY_PORT_NAME, "output-left",
            NULL),
        NULL, 0);
    data.right_out_port = pw_filter_add_port(data.filter,
        PW_DIRECTION_OUTPUT,
        PW_FILTER_PORT_FLAG_MAP_BUFFERS,
        sizeof(struct port),
        pw_properties_new(
            PW_KEY_FORMAT_DSP, "32 bit float mono audio",
            PW_KEY_PORT_NAME, "output-right",
            NULL),
        NULL, 0);

    data.midi_in_port = pw_filter_add_port(data.filter,
            PW_DIRECTION_INPUT,
            0,
            sizeof(struct port),
            pw_properties_new(
                PW_KEY_FORMAT_DSP, "8 bit raw midi",
                PW_KEY_PORT_NAME, "midi-in",
                NULL),
            NULL, 0);

    params[0] = spa_process_latency_build(&b,
        SPA_PARAM_ProcessLatency,
        &SPA_PROCESS_LATENCY_INFO_INIT(
            .ns = 10 * SPA_NSEC_PER_MSEC
        ));
    if (pw_filter_connect(data.filter,
            PW_FILTER_FLAG_RT_PROCESS,
            params, 1) < 0) {
        fprintf(stderr, "can't connect\n");
        return -1;
    }
    pw_main_loop_run(data.loop);
    pw_filter_destroy(data.filter);
    pw_main_loop_destroy(data.loop);
    pw_deinit();
    return 0;
}
