/*
 * windows_sim.c - PipeWire <-> UDP streaming bridge for PWAR
 *
 * (c) 2025 Philip K. Gisslow
 * This file is part of the PipeWire ASIO Relay (PWAR) project.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include "../protocol/pwar_packet.h"
#include "../protocol/pwar_router.h"

#include "latency_manager.h"

#define DEFAULT_STREAM_IP "127.0.0.1"
#define DEFAULT_STREAM_PORT 8321
#define SIM_PORT 8322

#define CHANNELS 2
#define BUFFER_SIZE 512

static int recv_sockfd;
static pthread_mutex_t packet_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t packet_cond = PTHREAD_COND_INITIALIZER;
static pwar_packet_t latest_packet;
static int packet_available = 0;
static pwar_router_t router;
static struct sockaddr_in servaddr;
static int sockfd;

static void setup_recv_socket(int port) {
    recv_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (recv_sockfd < 0) {
        perror("recv socket creation failed");
        exit(EXIT_FAILURE);
    }
    int rcvbuf = 1024 * 1024;
    setsockopt(recv_sockfd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));
    struct sockaddr_in recv_addr;
    memset(&recv_addr, 0, sizeof(recv_addr));
    recv_addr.sin_family = AF_INET;
    recv_addr.sin_addr.s_addr = INADDR_ANY;
    recv_addr.sin_port = htons(port);
    if (bind(recv_sockfd, (struct sockaddr *)&recv_addr, sizeof(recv_addr)) < 0) {
        perror("recv socket bind failed");
        exit(EXIT_FAILURE);
    }
}

static void *receiver_thread(void *userdata) {
    struct sched_param sp = { .sched_priority = 90 };
    pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp);
    pwar_packet_t packet;

    pwar_packet_t output_packets[32];
    uint32_t packets_to_send = 0;
    while (1) {
        ssize_t n = recvfrom(recv_sockfd, &packet, sizeof(packet), 0, NULL, NULL);
        if (n == (ssize_t)sizeof(packet)) {
            pthread_mutex_lock(&packet_mutex);
            latest_packet = packet;
            packet_available = 1;
            float output_buffers[CHANNELS * BUFFER_SIZE] = {0};
            uint32_t chunk_size = packet.n_samples;
            packet.num_packets = BUFFER_SIZE / chunk_size;
            latency_manager_process_packet_client(&packet);
            int samples_ready = pwar_router_process_streaming_packet(&router, &packet, output_buffers, BUFFER_SIZE, CHANNELS);
            if (samples_ready > 0) {
                uint32_t seq = packet.seq;

                latency_manager_start_audio_cbk_begin();
                // Process the output buffers as needed
                // Loop back.
                // But first copy channel 0 to channel 1 for testing
                for (uint32_t i = 0; i < samples_ready; ++i)
                    output_buffers[BUFFER_SIZE + i] = output_buffers[i];
                latency_manager_start_audio_cbk_end();

                pwar_router_send_buffer(&router, chunk_size, output_buffers, samples_ready, CHANNELS, output_packets, 32, &packets_to_send);

                uint64_t timestamp = latency_manager_timestamp_now();
                // Set seq for all packets in this buffer
                for (uint32_t i = 0; i < packets_to_send; ++i) {
                    output_packets[i].seq = seq;
                    output_packets[i].timestamp = timestamp;
                }
                for (uint32_t i = 0; i < packets_to_send; ++i) {
                    ssize_t sent = sendto(sockfd, &output_packets[i], sizeof(output_packets[i]), 0, (struct sockaddr *)&servaddr, sizeof(servaddr));
                    if (sent < 0) {
                        perror("sendto failed");
                    }
                }
            }
            pwar_latency_info_t latency_info;
            if (latency_manager_time_for_sending_latency_info(&latency_info)) {
                ssize_t sent = sendto(sockfd, &latency_info, sizeof(latency_info), 0, (struct sockaddr *)&servaddr, sizeof(servaddr));
                if (sent < 0) {
                    perror("sendto latency info failed");
                }
            }
            pthread_cond_signal(&packet_cond);
            pthread_mutex_unlock(&packet_mutex);
        }
    }
    return NULL;
}

int main() {
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) { perror("socket"); exit(1); }
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(DEFAULT_STREAM_PORT);
    servaddr.sin_addr.s_addr = inet_addr(DEFAULT_STREAM_IP);

    pwar_router_init(&router, CHANNELS);

    setup_recv_socket(SIM_PORT);
    pthread_t recv_thread;
    pthread_create(&recv_thread, NULL, receiver_thread, NULL);

    while (1) {
        sleep(1);
    }
    return 0;
}