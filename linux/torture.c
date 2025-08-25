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

#define TORTURE_PORT 8321
#define TORTURE_IP "192.168.66.3"

static int recv_sockfd;
static pthread_mutex_t packet_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t packet_cond = PTHREAD_COND_INITIALIZER;
static pwar_packet_t latest_packet;
static int packet_available = 0;

#define RT_STREAM_PACKET_FRAME_SIZE 128 // Define the frame size for the torture test

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
            //printf("[RECV] Got packet seq=%lu n_samples=%u\n", packet.seq, packet.n_samples);
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
    servaddr.sin_port = htons(TORTURE_PORT);
    servaddr.sin_addr.s_addr = inet_addr(TORTURE_IP);

    setup_recv_socket(TORTURE_PORT);
    pthread_t recv_thread;
    pthread_create(&recv_thread, NULL, receiver_thread, NULL);

    uint64_t seq = 0;
    while (1) {
        pwar_packet_t packet;
        packet.n_samples = RT_STREAM_PACKET_FRAME_SIZE;
        packet.seq = seq++;
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        packet.timestamp = (uint64_t)ts.tv_sec * 1000000000 + ts.tv_nsec;
        for (int i = 0; i < RT_STREAM_PACKET_FRAME_SIZE/2; ++i) {
            packet.samples[0][i] = (float)i;
            packet.samples[1][i] = (float)(RT_STREAM_PACKET_FRAME_SIZE - i);
        }
        ssize_t sent = sendto(sockfd, &packet, sizeof(packet), 0, (struct sockaddr *)&servaddr, sizeof(servaddr));
        if (sent != sizeof(packet)) {
            perror("sendto");
        } else {
            //printf("[SEND] Sent packet seq=%lu\n", packet.seq);
        }
        usleep(2600); // 2.6ms
    }
    return 0;
}
