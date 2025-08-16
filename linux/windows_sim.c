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

#define DEFAULT_STREAM_IP "127.0.0.1"
#define DEFAULT_STREAM_PORT 8321
#define SIM_PORT 8322

static int recv_sockfd;
static pthread_mutex_t packet_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t packet_cond = PTHREAD_COND_INITIALIZER;
static pwar_packet_t latest_packet;
static int packet_available = 0;

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
    while (1) {
        ssize_t n = recvfrom(recv_sockfd, &packet, sizeof(packet), 0, NULL, NULL);
        if (n == (ssize_t)sizeof(packet)) {
            pthread_mutex_lock(&packet_mutex);
            latest_packet = packet;
            packet_available = 1;
            pthread_cond_signal(&packet_cond);
            pthread_mutex_unlock(&packet_mutex);
        }
    }
    return NULL;
}

int main() {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) { perror("socket"); exit(1); }
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(DEFAULT_STREAM_PORT);
    servaddr.sin_addr.s_addr = inet_addr(DEFAULT_STREAM_IP);

    setup_recv_socket(SIM_PORT);
    pthread_t recv_thread;
    pthread_create(&recv_thread, NULL, receiver_thread, NULL);

    while (1) {
        // How to send:
        //ssize_t sent = sendto(sockfd, &packet, sizeof(packet), 0, (struct sockaddr *)&servaddr, sizeof(servaddr));
    }
    return 0;
}