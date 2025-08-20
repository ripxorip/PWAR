#include "libpwar.h"
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

#include "latency_manager.h"

#include "pwar_packet.h"
#include "pwar_router.h"
#include "pwar_rcv_buffer.h"

#define DEFAULT_STREAM_IP "192.168.66.3"
#define DEFAULT_STREAM_PORT 8321

#define MAX_BUFFER_SIZE 4096
#define NUM_CHANNELS 2

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
    float sine_phase;
    uint8_t passthrough_test; // Add passthrough_test flag
    uint8_t oneshot_mode; // Add oneshot_mode flag
    uint32_t seq;
    int sockfd;
    struct sockaddr_in servaddr;
    int recv_sockfd;

    pthread_mutex_t packet_mutex;
    pthread_cond_t packet_cond;
    pwar_packet_t latest_packet;
    int packet_available;

    pwar_router_t linux_router;
    pthread_mutex_t pwar_rcv_mutex; // Mutex for receive buffer
};

static void setup_recv_socket(struct data *data, int port);
static void *receiver_thread(void *userdata);

static void setup_socket(struct data *data, const char *ip, int port);

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
    char recv_buffer[sizeof(pwar_packet_t) > sizeof(pwar_latency_info_t) ? sizeof(pwar_packet_t) : sizeof(pwar_latency_info_t)];
    float linux_output_buffers[NUM_CHANNELS * MAX_BUFFER_SIZE] = {0};

    while (1) {
        ssize_t n = recvfrom(data->recv_sockfd, recv_buffer, sizeof(recv_buffer), 0, NULL, NULL);
        if (n == (ssize_t)sizeof(pwar_packet_t)) {
            pwar_packet_t *packet = (pwar_packet_t *)recv_buffer;
            latency_manager_process_packet_server(packet);
            if (data->oneshot_mode) {
                pthread_mutex_lock(&data->packet_mutex);
                data->latest_packet = *packet;
                data->packet_available = 1;
                pthread_cond_signal(&data->packet_cond);
                pthread_mutex_unlock(&data->packet_mutex);
            } 
            else {
                int samples_ready = pwar_router_process_packet(&data->linux_router, packet, linux_output_buffers, MAX_BUFFER_SIZE, NUM_CHANNELS);
                if (samples_ready > 0) {
                    pthread_mutex_lock(&data->pwar_rcv_mutex); // Lock before buffer add
                    pwar_rcv_buffer_add_buffer(linux_output_buffers, samples_ready, NUM_CHANNELS);
                    pthread_mutex_unlock(&data->pwar_rcv_mutex); // Unlock after buffer add
                }
            }
        } else if (n == (ssize_t)sizeof(pwar_latency_info_t)) {
            pwar_latency_info_t *latency_info = (pwar_latency_info_t *)recv_buffer;
            latency_manager_handle_latency_info(latency_info);
        }
    }
    return NULL;
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
    pwar_packet_t packet;
    packet.seq = data->seq++;
    packet.n_samples = n_samples;
    // Just stream the first channel for now.. FIXME: This should be updated to handle multiple channels properly in the future
    memcpy(packet.samples[0], samples, n_samples * sizeof(float));

    packet.timestamp = latency_manager_timestamp_now();
    packet.seq_timestamp = packet.timestamp; // Set seq_timestamp to the same value as timestamp
    if (sendto(data->sockfd, &packet, sizeof(packet), 0, (struct sockaddr *)&data->servaddr, sizeof(data->servaddr)) < 0) {
        perror("sendto failed");
    }
}

static void process_one_shot(void *userdata, float *in, uint32_t n_samples, float *left_out, float *right_out) {
    struct data *data = (struct data *)userdata;
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
        // Wait for packet or timeout (no ping-pong)
        int rc = pthread_cond_timedwait(&data->packet_cond, &data->packet_mutex, &ts);
        if (rc == ETIMEDOUT)
            break;
    }
    if (data->packet_available) {
        if (left_out)
            memcpy(left_out, data->latest_packet.samples[0], n_samples * sizeof(float));
        if (right_out)
            memcpy(right_out, data->latest_packet.samples[1], n_samples * sizeof(float));
        got_packet = 1;
        data->packet_available = 0;
    }
    pthread_mutex_unlock(&data->packet_mutex);
    if (!got_packet) {
        printf("\033[0;31m--- ERROR -- No valid packet received, outputting silence\n");
        printf("I wanted seq: %u and got seq: %lu\033[0m\n", data->seq - 1, data->latest_packet.seq);
        if (left_out)
            memset(left_out, 0, n_samples * sizeof(float));
        if (right_out)
            memset(right_out, 0, n_samples * sizeof(float));
    }
}

static void process_ping_pong(void *userdata, float *in, uint32_t n_samples, float *left_out, float *right_out) {
    struct data *data = (struct data *)userdata;

    // Create a packet for the input samples
    pwar_packet_t packet;
    packet.seq = data->seq++;
    packet.n_samples = n_samples;

    // Just stream the first channel for now.. FIXME: This should be updated to handle multiple channels properly in the future
    memcpy(packet.samples[0], in, n_samples * sizeof(float));

    packet.timestamp = latency_manager_timestamp_now();
    packet.seq_timestamp = packet.timestamp; // Set seq_timestamp to the same value as timestamp

    /* Lock to prevent the response being received too soon */
    pthread_mutex_lock(&data->pwar_rcv_mutex); // Lock before get_chunk

    if (sendto(data->sockfd, &packet, sizeof(packet), 0, (struct sockaddr *)&data->servaddr, sizeof(data->servaddr)) < 0) {
        perror("sendto failed");
    }

    float linux_rcv_buffers[NUM_CHANNELS * n_samples];
    memset(linux_rcv_buffers, 0, sizeof(linux_rcv_buffers));
    // Get the chunk from n-1 (ping-pong)
    pwar_rcv_get_chunk(linux_rcv_buffers, NUM_CHANNELS, n_samples);

    pthread_mutex_unlock(&data->pwar_rcv_mutex); // Unlock after get_chunk

    if (left_out)
        memcpy(left_out, linux_rcv_buffers, n_samples * sizeof(float));
    if (right_out)
        memcpy(right_out, linux_rcv_buffers + n_samples, n_samples * sizeof(float));
}

static void on_process(void *userdata, struct spa_io_position *position) {
    struct data *data = (struct data *)userdata;
    float *in = pw_filter_get_dsp_buffer(data->in_port, position->clock.duration);
    float *left_out = pw_filter_get_dsp_buffer(data->left_out_port, position->clock.duration);
    float *right_out = pw_filter_get_dsp_buffer(data->right_out_port, position->clock.duration);

    uint32_t n_samples = position->clock.duration;
    if (data->passthrough_test) {
        if (left_out)
            memcpy(left_out, in, n_samples * sizeof(float));
        if (right_out)
            memcpy(right_out, in, n_samples * sizeof(float));
        return;
    }

    if (data->oneshot_mode) {
        // Use one-shot processing, i.e. Linux send, Windows process, Linux receive in one go
        process_one_shot(data, in, n_samples, left_out, right_out);
    }
    else {
        // Use ping-pong processing, i.e. Linux send, Windows process, Linux receive in chunks
        process_ping_pong(data, in, n_samples, left_out, right_out);
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


int pwar_cli_run(const pwar_config_t *config) {
    char latency[32];
    snprintf(latency, sizeof(latency), "%d/48000", config->buffer_size);
    setenv("PIPEWIRE_LATENCY", latency, 1);

    struct data data;
    memset(&data, 0, sizeof(data));
    const struct spa_pod *params[1];
    uint8_t buffer[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

    setup_socket(&data, config->stream_ip, config->stream_port);
    setup_recv_socket(&data, DEFAULT_STREAM_PORT);
    pthread_mutex_init(&data.packet_mutex, NULL);
    pthread_cond_init(&data.packet_cond, NULL);
    data.packet_available = 0;
    pthread_mutex_init(&data.pwar_rcv_mutex, NULL);
    pthread_t recv_thread;
    pthread_create(&recv_thread, NULL, receiver_thread, &data);
    pw_init(NULL, NULL);
    data.loop = pw_main_loop_new(NULL);
    pw_loop_add_signal(pw_main_loop_get_loop(data.loop), SIGINT, do_quit, &data);
    pw_loop_add_signal(pw_main_loop_get_loop(data.loop), SIGTERM, do_quit, &data);
    data.passthrough_test = config->passthrough_test;
    data.oneshot_mode = config->oneshot_mode;
    data.sine_phase = 0.0f;
    pwar_router_init(&data.linux_router, NUM_CHANNELS);
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
