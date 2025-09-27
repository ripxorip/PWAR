// Core PWAR Library - Audio backend agnostic implementation
// Contains the core PWAR protocol logic without direct audio API dependencies

#define _POSIX_C_SOURCE 199309L  // Enable POSIX time functions

#include "libpwar.h"
#include "audio_backend.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <unistd.h>

#include "../protocol/latency_manager.h"
#include "../protocol/pwar_packet.h"
#include "../protocol/pwar_ring_buffer.h"

#define DEFAULT_STREAM_IP "192.168.66.3"
#define DEFAULT_STREAM_PORT 8321
#define MAX_BUFFER_SIZE 4096
#define NUM_CHANNELS 2

// Global data for GUI mode
static struct pwar_core_data *g_pwar_data = NULL;
static pthread_t g_recv_thread;
static int g_pwar_initialized = 0;
static int g_pwar_running = 0;
static pwar_config_t g_current_config;

// Core PWAR data structure (audio backend agnostic)
struct pwar_core_data {
    // Audio backend
    audio_backend_t *audio_backend;
    
    // PWAR configuration
    pwar_config_t config;
    
    // Network
    int sockfd;
    struct sockaddr_in servaddr;
    int recv_sockfd;
    
    // PWAR protocol state
    uint32_t seq;
    
    // Buffering for Windows packet accumulation
    float *accumulation_buffer;          // Buffer to accumulate device buffers
    uint32_t accumulated_samples;        // Number of samples currently accumulated
    uint32_t packets_per_send;           // How many device buffers per Windows packet
    
    uint32_t current_windows_buffer_size;
    volatile int should_stop;
};

// Forward declarations
static void setup_socket(struct pwar_core_data *data, const char *ip, int port);
static void setup_recv_socket(struct pwar_core_data *data, int port);
static void *receiver_thread(void *userdata);
static void audio_process_callback(float *in, float *out_left, float *out_right, 
                                 uint32_t n_samples, void *userdata);
static void process_audio(struct pwar_core_data *data, float *in, uint32_t n_samples, 
                            float *left_out, float *right_out);

static void setup_socket(struct pwar_core_data *data, const char *ip, int port) {
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

static void setup_recv_socket(struct pwar_core_data *data, int port) {
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

    struct pwar_core_data *data = (struct pwar_core_data *)userdata;
    char recv_buffer[sizeof(pwar_packet_t)];
    float output_buffers[NUM_CHANNELS * MAX_BUFFER_SIZE] = {0};

    // Set socket timeout to allow periodic checking of should_stop
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000; // 100ms timeout
    if (setsockopt(data->recv_sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("Warning: Failed to set socket timeout");
    }

    while (!data->should_stop) {
        ssize_t n = recvfrom(data->recv_sockfd, recv_buffer, sizeof(recv_buffer), 0, NULL, NULL);
        
        if (n == (ssize_t)sizeof(pwar_packet_t)) {
            pwar_packet_t *packet = (pwar_packet_t *)recv_buffer;
            latency_manager_process_packet(packet);
            data->current_windows_buffer_size = packet->n_samples;
            
            pwar_ring_buffer_push(packet->samples, packet->n_samples, NUM_CHANNELS);
            latency_manager_report_ring_buffer_fill_level(pwar_ring_buffer_get_available());
        } else if (n < 0) {
            // Check if it's a timeout (expected) vs a real error
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Timeout is expected - just continue and check should_stop
                continue;
            } else if (!data->should_stop) {
                // Only print error if we're not shutting down
                perror("recvfrom error");
            }
        }
    }
    return NULL;
}

static void audio_process_callback(float *in, float *out_left, float *out_right, 
                                 uint32_t n_samples, void *userdata) {
    struct pwar_core_data *data = (struct pwar_core_data *)userdata;
    
    if (data->config.passthrough_test) {
        // Local passthrough test - just copy input to output
        if (out_left) memcpy(out_left, in, n_samples * sizeof(float));
        if (out_right) memcpy(out_right, in, n_samples * sizeof(float));
        return;
    }

    process_audio(data, in, n_samples, out_left, out_right);
}

static void process_audio(struct pwar_core_data *data, float *in, uint32_t n_samples, 
                            float *left_out, float *right_out) {
    // Accumulate input samples in interleaved format into the accumulation buffer
    for (uint32_t i = 0; i < n_samples; i++) {
        uint32_t buffer_idx = data->accumulated_samples + i;
        data->accumulation_buffer[buffer_idx * NUM_CHANNELS + 0] = in[i];  // Left channel
        data->accumulation_buffer[buffer_idx * NUM_CHANNELS + 1] = in[i];  // Right channel
    }
    
    data->accumulated_samples += n_samples;
    
    // Check if we have accumulated enough samples for a Windows packet
    if (data->accumulated_samples >= data->config.windows_packet_size) {
        // Send accumulated packet to Windows
        pwar_packet_t packet;
        packet.n_samples = data->config.windows_packet_size;
        
        // Copy accumulated samples to packet
        memcpy(packet.samples, data->accumulation_buffer, 
               data->config.windows_packet_size * NUM_CHANNELS * sizeof(float));
        
        packet.t1_linux_send = latency_manager_timestamp_now();
        
        if (sendto(data->sockfd, &packet, sizeof(packet), 0, 
                   (struct sockaddr *)&data->servaddr, sizeof(data->servaddr)) < 0) {
            perror("sendto failed");
        }
        
        // Reset accumulation buffer
        data->accumulated_samples = 0;
        memset(data->accumulation_buffer, 0, 
               data->config.windows_packet_size * NUM_CHANNELS * sizeof(float));
    }
    
    // Get processed samples from ring buffer for current device buffer size
    float rcv_buffers[NUM_CHANNELS * n_samples];
    memset(rcv_buffers, 0, sizeof(rcv_buffers));
    
    pwar_ring_buffer_pop(rcv_buffers, n_samples, NUM_CHANNELS);
    
    // Convert from interleaved format to separate channel outputs
    for (uint32_t i = 0; i < n_samples; i++) {
        if (left_out) left_out[i] = rcv_buffers[i * NUM_CHANNELS + 0];
        if (right_out) right_out[i] = rcv_buffers[i * NUM_CHANNELS + 1];
    }
}

// Extract common initialization logic
static int init_core_data(struct pwar_core_data *data, const pwar_config_t *config) {
    memset(data, 0, sizeof(struct pwar_core_data));
    data->config = *config;
    
    setup_socket(data, config->stream_ip, config->stream_port);
    setup_recv_socket(data, DEFAULT_STREAM_PORT);
    
    data->seq = 0;
    
    // Calculate packets per send and allocate accumulation buffer
    data->packets_per_send = config->windows_packet_size / config->device_buffer_size;
    data->accumulated_samples = 0;
    
    // Allocate accumulation buffer for interleaved samples
    size_t buffer_size = config->windows_packet_size * NUM_CHANNELS * sizeof(float);
    data->accumulation_buffer = (float*)malloc(buffer_size);
    if (!data->accumulation_buffer) {
        fprintf(stderr, "Failed to allocate accumulation buffer\n");
        return -1;
    }
    memset(data->accumulation_buffer, 0, buffer_size);
    
    // Initialize ring buffer with configured depth and Windows packet size
    pwar_ring_buffer_init(config->ring_buffer_depth, NUM_CHANNELS, config->windows_packet_size);
    
    // Create appropriate audio backend using unified factory
    if (!audio_backend_is_available(config->backend_type)) {
        fprintf(stderr, "Audio backend type %d is not available (not compiled in)\n", config->backend_type);
        free(data->accumulation_buffer);
        return -1;
    }
    
    data->audio_backend = audio_backend_create(config->backend_type);
    
    if (!data->audio_backend) {
        fprintf(stderr, "Failed to create audio backend\n");
        free(data->accumulation_buffer);
        return -1;
    }
    
    // Initialize the audio backend
    if (audio_backend_init(data->audio_backend, &config->audio_config, 
                          audio_process_callback, data) < 0) {
        fprintf(stderr, "Failed to initialize audio backend\n");
        audio_backend_cleanup(data->audio_backend);
        data->audio_backend = NULL;
        free(data->accumulation_buffer);
        return -1;
    }
    
    // Initialize latency manager with Windows packet size
    latency_manager_init(config->audio_config.sample_rate, config->windows_packet_size, data->audio_backend->ops->get_latency(data->audio_backend));
    
    return 0;
}

// Signal handler for CLI mode
static volatile int cli_keep_running = 1;
static void cli_sigint_handler(int sig) {
    cli_keep_running = 0;
}

// Public API Implementation
int pwar_requires_restart(const pwar_config_t *old_config, const pwar_config_t *new_config) {
    if (old_config->device_buffer_size != new_config->device_buffer_size ||
        old_config->windows_packet_size != new_config->windows_packet_size ||
        old_config->ring_buffer_depth != new_config->ring_buffer_depth ||
        strcmp(old_config->stream_ip, new_config->stream_ip) != 0 ||
        old_config->stream_port != new_config->stream_port ||
        old_config->backend_type != new_config->backend_type) {
        return 1;
    }
    return 0;
}

int pwar_update_config(const pwar_config_t *config) {
    if (!g_pwar_initialized) {
        return -1;
    }

    if (pwar_requires_restart(&g_current_config, config)) {
        return -2; // Signal that restart is needed
    }

    // Apply runtime-changeable settings
    g_pwar_data->config.passthrough_test = config->passthrough_test;
    g_current_config = *config;
    
    return 0;
}

int pwar_init(const pwar_config_t *config) {
    if (g_pwar_initialized) {
        return -1;
    }

    g_current_config = *config;

    g_pwar_data = malloc(sizeof(struct pwar_core_data));
    if (!g_pwar_data) {
        return -1;
    }

    if (init_core_data(g_pwar_data, config) < 0) {
        free(g_pwar_data);
        g_pwar_data = NULL;
        return -1;
    }

    // Start receiver thread
    g_pwar_data->should_stop = 0;
    pthread_create(&g_recv_thread, NULL, receiver_thread, g_pwar_data);

    g_pwar_initialized = 1;
    return 0;
}

int pwar_start(void) {
    if (!g_pwar_initialized || g_pwar_running) {
        return -1;
    }

    if (audio_backend_start(g_pwar_data->audio_backend) < 0) {
        return -1;
    }

    g_pwar_running = 1;
    return 0;
}

int pwar_stop(void) {
    if (!g_pwar_running) {
        return -1;
    }

    audio_backend_stop(g_pwar_data->audio_backend);
    g_pwar_running = 0;
    return 0;
}

void pwar_cleanup(void) {
    if (g_pwar_running) {
        pwar_stop();
    }

    if (g_pwar_initialized) {
        g_pwar_data->should_stop = 1;
        pthread_cancel(g_recv_thread);
        pthread_join(g_recv_thread, NULL);

        if (g_pwar_data->audio_backend) {
            audio_backend_cleanup(g_pwar_data->audio_backend);
            g_pwar_data->audio_backend = NULL;
        }

        if (g_pwar_data->sockfd > 0) {
            close(g_pwar_data->sockfd);
        }
        if (g_pwar_data->recv_sockfd > 0) {
            close(g_pwar_data->recv_sockfd);
        }

        if (g_pwar_data->accumulation_buffer) {
            free(g_pwar_data->accumulation_buffer);
            g_pwar_data->accumulation_buffer = NULL;
        }

        pwar_ring_buffer_free();

        free(g_pwar_data);
        g_pwar_data = NULL;
        g_pwar_initialized = 0;
    }
}

int pwar_is_running(void) {
    return g_pwar_running;
}

int pwar_cli_run(const pwar_config_t *config) {
    struct pwar_core_data data;
    pthread_t recv_thread;

    // Set up signal handler for CLI mode
    signal(SIGINT, cli_sigint_handler);
    signal(SIGTERM, cli_sigint_handler);

    // Initialize core data
    if (init_core_data(&data, config) < 0) {
        return -1;
    }

    // Start receiver thread
    data.should_stop = 0;
    pthread_create(&recv_thread, NULL, receiver_thread, &data);

    // Start audio backend
    if (audio_backend_start(data.audio_backend) < 0) {
        fprintf(stderr, "Failed to start audio backend\n");
        data.should_stop = 1;
        pthread_join(recv_thread, NULL);
        audio_backend_cleanup(data.audio_backend);
        return -1;
    }

    printf("PWAR CLI started with %s backend. Press Ctrl+C to stop.\n",
           config->backend_type == AUDIO_BACKEND_ALSA ? "ALSA" : 
           config->backend_type == AUDIO_BACKEND_PIPEWIRE ? "PipeWire" : "Simulated");

    // Wait for shutdown signal
    while (cli_keep_running) {
        struct timespec sleep_time = {0, 100000000}; // 100ms
        nanosleep(&sleep_time, NULL);
    }

    printf("\nShutting down PWAR CLI...\n");

    // Cleanup
    data.should_stop = 1;
    audio_backend_stop(data.audio_backend);
    pthread_join(recv_thread, NULL);
    audio_backend_cleanup(data.audio_backend);

    if (data.sockfd > 0) close(data.sockfd);
    if (data.recv_sockfd > 0) close(data.recv_sockfd);
    
    if (data.accumulation_buffer) {
        free(data.accumulation_buffer);
    }

    pwar_ring_buffer_free();

    return 0;
}

void pwar_get_latency_metrics(pwar_latency_metrics_t *metrics) {
    if (!metrics) return;
    
    if (g_pwar_initialized && g_pwar_running) {
        latency_manger_get_current_metrics(metrics);
    } else {
        // Return zeros if not running
        memset(metrics, 0, sizeof(pwar_latency_metrics_t));
    }
}

uint32_t pwar_get_current_windows_buffer_size(void) {
    if (g_pwar_initialized && g_pwar_running && g_pwar_data) {
        return g_pwar_data->current_windows_buffer_size;
    }
    return 0;
}
