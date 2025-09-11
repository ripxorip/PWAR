/*
 * client_simulator.cpp - PWAR Client Simulator (Windows)
 *
 * Simulates a PWAR client (like Windows ASIO driver) for testing
 * Receives audio from PWAR server, processes it, and sends it back
 *
 * (c) 2025 Philip K. Gisslow
 * This file is part of the PipeWire ASIO Relay (PWAR) project.
 */

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cerrno>
#include <ctime>
#include <csignal>

extern "C" {
#include "../../protocol/pwar_packet.h"
#include "../../protocol/latency_manager.h"
}

#pragma comment(lib, "ws2_32.lib")

#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR,12)
#endif

// Default configuration (matching your existing setup)
#define DEFAULT_SERVER_IP "196.168.66.2"
#define DEFAULT_SERVER_PORT 8321
#define DEFAULT_CLIENT_PORT 8321
#define DEFAULT_CHANNELS 2
#define DEFAULT_PACKET_SIZE 512

// Configuration structure
typedef struct {
    char server_ip[64];
    int server_port;
    int client_port;
    int channels;
    int packet_size;
    int verbose;
} client_config_t;

// Global state
static SOCKET recv_sockfd = INVALID_SOCKET;
static SOCKET send_sockfd = INVALID_SOCKET;
static struct sockaddr_in servaddr;
static volatile int keep_running = 1;
static client_config_t config;

static void print_usage(const char *program_name) {
    printf("PWAR Client Simulator - Simulates a PWAR client for testing\n\n");
    printf("Usage: %s [options]\n", program_name);
    printf("Options:\n");
    printf("  -s, --server <ip>      Server IP address (default: %s)\n", DEFAULT_SERVER_IP);
    printf("  --server-port <port>   Server port (default: %d)\n", DEFAULT_SERVER_PORT);
    printf("  -c, --client-port <port> Client listening port (default: %d)\n", DEFAULT_CLIENT_PORT);
    printf("  -p, --packet-size <size> Packet size in samples (default: %d)\n", DEFAULT_PACKET_SIZE);
    printf("  -n, --channels <count> Number of channels (default: %d)\n", DEFAULT_CHANNELS);
    printf("  -v, --verbose          Enable verbose output\n");
    printf("  -h, --help             Show this help message\n");
    printf("\nExamples:\n");
    printf("  %s                           # Connect to localhost with defaults\n", program_name);
    printf("  %s -s 192.168.1.100 --server-port 9000  # Connect to remote server\n", program_name);
    printf("  %s -v -p 256 -c 1            # Verbose mode, smaller packets, mono\n", program_name);
    printf("\nDescription:\n");
    printf("  This simulator acts like a PWAR client (e.g., Windows ASIO driver).\n");
    printf("  It receives audio packets from a PWAR server, processes them,\n");
    printf("  and sends them back, creating a loopback test environment.\n");
}

static int parse_arguments(int argc, char *argv[], client_config_t *cfg) {
    // Set defaults
    strcpy_s(cfg->server_ip, sizeof(cfg->server_ip), DEFAULT_SERVER_IP);
    cfg->server_port = DEFAULT_SERVER_PORT;
    cfg->client_port = DEFAULT_CLIENT_PORT;
    cfg->channels = DEFAULT_CHANNELS;
    cfg->packet_size = DEFAULT_PACKET_SIZE;
    cfg->verbose = 0;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return -1;
        } else if ((strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--server") == 0) && i + 1 < argc) {
            strcpy_s(cfg->server_ip, sizeof(cfg->server_ip), argv[++i]);
        } else if (strcmp(argv[i], "--server-port") == 0 && i + 1 < argc) {
            cfg->server_port = atoi(argv[++i]);
        } else if ((strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--client-port") == 0) && i + 1 < argc) {
            cfg->client_port = atoi(argv[++i]);
        } else if ((strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--packet-size") == 0) && i + 1 < argc) {
            cfg->packet_size = atoi(argv[++i]);
        } else if ((strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "--channels") == 0) && i + 1 < argc) {
            cfg->channels = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            cfg->verbose = 1;
        } else {
            fprintf(stderr, "Unknown argument: %s\n", argv[i]);
            print_usage(argv[0]);
            return -1;
        }
    }
    
    // Validate configuration
    if (cfg->server_port <= 0 || cfg->server_port > 65535) {
        fprintf(stderr, "Invalid server port: %d\n", cfg->server_port);
        return -1;
    }
    if (cfg->client_port <= 0 || cfg->client_port > 65535) {
        fprintf(stderr, "Invalid client port: %d\n", cfg->client_port);
        return -1;
    }
    if (cfg->channels < 1 || cfg->channels > 8) {
        fprintf(stderr, "Invalid channel count: %d (must be 1-8)\n", cfg->channels);
        return -1;
    }
    if (cfg->packet_size < 32 || cfg->packet_size > 4096) {
        fprintf(stderr, "Invalid packet size: %d (must be 32-4096)\n", cfg->packet_size);
        return -1;
    }
    
    return 0;
}

static void setup_recv_socket(int port) {
    recv_sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (recv_sockfd == INVALID_SOCKET) {
        fprintf(stderr, "recv socket creation failed: %d\n", WSAGetLastError());
        exit(EXIT_FAILURE);
    }
    
    // Disable ICMP port unreachable messages
    BOOL new_behavior = FALSE;
    DWORD bytes_returned = 0;
    WSAIoctl(recv_sockfd, SIO_UDP_CONNRESET, &new_behavior, sizeof(new_behavior),
             NULL, 0, &bytes_returned, NULL, NULL);
    
    // Set socket options for better performance
    int rcvbuf = 1024 * 1024;
    if (setsockopt(recv_sockfd, SOL_SOCKET, SO_RCVBUF, (char*)&rcvbuf, sizeof(rcvbuf)) == SOCKET_ERROR) {
        fprintf(stderr, "Warning: Failed to set receive buffer size: %d\n", WSAGetLastError());
    }
    
    // Set socket timeout to allow periodic checking of keep_running flag
    DWORD timeout = 100; // 100ms timeout
    if (setsockopt(recv_sockfd, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout)) == SOCKET_ERROR) {
        fprintf(stderr, "Warning: Failed to set socket timeout: %d\n", WSAGetLastError());
    }
    
    struct sockaddr_in recv_addr;
    memset(&recv_addr, 0, sizeof(recv_addr));
    recv_addr.sin_family = AF_INET;
    recv_addr.sin_addr.s_addr = INADDR_ANY;
    recv_addr.sin_port = htons((u_short)port);
    
    if (bind(recv_sockfd, (struct sockaddr *)&recv_addr, sizeof(recv_addr)) == SOCKET_ERROR) {
        fprintf(stderr, "recv socket bind failed: %d\n", WSAGetLastError());
        exit(EXIT_FAILURE);
    }
    
    if (config.verbose) {
        printf("[Client Simulator] Listening on port %d\n", port);
    }
}

static void setup_send_socket(const char *server_ip, int server_port) {
    send_sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (send_sockfd == INVALID_SOCKET) {
        fprintf(stderr, "send socket creation failed: %d\n", WSAGetLastError());
        exit(EXIT_FAILURE);
    }
    
    // Disable ICMP port unreachable messages
    BOOL new_behavior = FALSE;
    DWORD bytes_returned = 0;
    WSAIoctl(send_sockfd, SIO_UDP_CONNRESET, &new_behavior, sizeof(new_behavior),
             NULL, 0, &bytes_returned, NULL, NULL);
    
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons((u_short)server_port);
    
    if (inet_pton(AF_INET, server_ip, &servaddr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid server IP address: %s\n", server_ip);
        exit(EXIT_FAILURE);
    }
    
    if (config.verbose) {
        printf("[Client Simulator] Sending to %s:%d\n", server_ip, server_port);
    }
}

static DWORD WINAPI receiver_thread(LPVOID userdata) {
    // Set thread priority to high for better real-time performance
    if (!SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST)) {
        fprintf(stderr, "Warning: Failed to set high thread priority: %lu\n", GetLastError());
    }
    
    pwar_packet_t packet;
    pwar_packet_t response_packet;
    uint64_t packets_processed = 0;
    
    printf("[Client Simulator] Receiver thread started\n");
    
    while (keep_running) {
        int n = recvfrom(recv_sockfd, (char*)&packet, sizeof(packet), 0, NULL, NULL);
        
        if (n == sizeof(packet)) {
            // Set Windows receive timestamp
            packet.t2_windows_recv = latency_manager_timestamp_now();
            
            // Copy input to output 1:1 (no audio processing)
            response_packet = packet; // Copy the whole structure including samples
            
            // Set Windows send timestamp
            response_packet.t3_windows_send = latency_manager_timestamp_now();
            
            // Send response packet back to server
            int sent = sendto(send_sockfd, (char*)&response_packet, sizeof(response_packet), 0, 
                            (struct sockaddr *)&servaddr, sizeof(servaddr));
            if (sent == SOCKET_ERROR) {
                int error = WSAGetLastError();
                if (error != WSAEWOULDBLOCK) {
                    fprintf(stderr, "sendto failed: %d\n", error);
                }
            }
            
            packets_processed++;
            if (config.verbose && packets_processed % 1000 == 0) {
                printf("[Client Simulator] Processed %llu packets\n", 
                       (unsigned long long)packets_processed);
            }
        } else if (n == SOCKET_ERROR) {
            int error = WSAGetLastError();
            // Check if it's a timeout (expected) vs a real error
            if (error == WSAETIMEDOUT || error == WSAEWOULDBLOCK) {
                // Timeout is expected - just continue and check keep_running
                continue;
            } else if (keep_running) {
                // Only print error if we're not shutting down
                fprintf(stderr, "recvfrom error: %d\n", error);
            }
        }
        // If n == 0 or some other size, just continue
    }
    
    printf("[Client Simulator] Receiver thread stopped\n");
    return 0;
}

static BOOL WINAPI console_ctrl_handler(DWORD ctrl_type) {
    switch (ctrl_type) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
        printf("\n[Client Simulator] Received shutdown signal, shutting down...\n");
        keep_running = 0;
        return TRUE;
    default:
        return FALSE;
    }
}

int main(int argc, char *argv[]) {
    printf("PWAR Client Simulator - Testing tool for PWAR protocol (Windows)\n");
    printf("Simulates a PWAR client (like Windows ASIO driver)\n\n");
    
    // Initialize Winsock
    WSADATA wsa_data;
    int wsa_result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (wsa_result != 0) {
        fprintf(stderr, "WSAStartup failed: %d\n", wsa_result);
        return 1;
    }
    
    // Parse command line arguments
    if (parse_arguments(argc, argv, &config) < 0) {
        WSACleanup();
        return 1;
    }
    
    // Print configuration
    printf("Configuration:\n");
    printf("  Server:        %s:%d\n", config.server_ip, config.server_port);
    printf("  Client port:   %d\n", config.client_port);
    printf("  Channels:      %d\n", config.channels);
    printf("  Packet size:   %d samples\n", config.packet_size);
    printf("  Verbose:       %s\n", config.verbose ? "enabled" : "disabled");
    printf("\n");
    
    // Set up console control handler for graceful shutdown
    if (!SetConsoleCtrlHandler(console_ctrl_handler, TRUE)) {
        fprintf(stderr, "Warning: Failed to set console control handler\n");
    }
    
    // Set up networking
    setup_recv_socket(config.client_port);
    setup_send_socket(config.server_ip, config.server_port);
    
    // Start receiver thread
    HANDLE recv_thread = CreateThread(NULL, 0, receiver_thread, NULL, 0, NULL);
    if (recv_thread == NULL) {
        fprintf(stderr, "Failed to create receiver thread: %lu\n", GetLastError());
        closesocket(recv_sockfd);
        closesocket(send_sockfd);
        WSACleanup();
        return 1;
    }
    
    printf("[Client Simulator] Started successfully. Press Ctrl+C to stop.\n");
    printf("[Client Simulator] Waiting for audio packets from PWAR server...\n");
    
    // Main loop
    while (keep_running) {
        Sleep(100); // 100ms
    }
    
    // Cleanup
    printf("[Client Simulator] Shutting down...\n");
    keep_running = 0;
    WaitForSingleObject(recv_thread, 5000); // Wait up to 5 seconds for thread to finish
    CloseHandle(recv_thread);
    
    if (recv_sockfd != INVALID_SOCKET) {
        closesocket(recv_sockfd);
    }
    if (send_sockfd != INVALID_SOCKET) {
        closesocket(send_sockfd);
    }
    
    WSACleanup();
    
    printf("[Client Simulator] Shutdown complete\n");
    return 0;
}