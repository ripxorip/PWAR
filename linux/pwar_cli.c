/*
 * pwar_cli.c - CLI frontend for PipeWire <-> UDP streaming bridge (PWAR)
 *
 * (c) 2025 Philip K. Gisslow
 * This file is part of the PipeWire ASIO Relay (PWAR) project.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libpwar.h"

#define DEFAULT_STREAM_IP "192.168.66.3"
#define DEFAULT_STREAM_PORT 8321
#define DEFAULT_BUFFER_SIZE 64

int main(int argc, char *argv[]) {
    pwar_config_t config;
    memset(&config, 0, sizeof(config));
    strncpy(config.stream_ip, DEFAULT_STREAM_IP, sizeof(config.stream_ip) - 1);
    config.stream_port = DEFAULT_STREAM_PORT;
    config.passthrough_test = 0;
    config.oneshot_mode = 0;
    config.buffer_size = DEFAULT_BUFFER_SIZE;

    for (int i = 1; i < argc; ++i) {
        if ((strcmp(argv[i], "--ip") == 0 || strcmp(argv[i], "-i") == 0) && i + 1 < argc) {
            strncpy(config.stream_ip, argv[++i], sizeof(config.stream_ip) - 1);
            config.stream_ip[sizeof(config.stream_ip) - 1] = '\0';
        } else if ((strcmp(argv[i], "--port") == 0 || (strcmp(argv[i], "-p") == 0)) && i + 1 < argc) {
            config.stream_port = atoi(argv[++i]);
        } else if ((strcmp(argv[i], "--passthrough_test") == 0) || (strcmp(argv[i], "-pt") == 0)) {
            config.passthrough_test = 1;
        } else if ((strcmp(argv[i], "--oneshot") == 0)) {
            config.oneshot_mode = 1;
        } else if ((strcmp(argv[i], "--buffer_size") == 0 || strcmp(argv[i], "-b") == 0) && i + 1 < argc) {
            config.buffer_size = atoi(argv[++i]);
        }
    }

    printf("Starting PWAR with config:\n");
    printf("  Stream IP: %s\n", config.stream_ip);
    printf("  Stream Port: %d\n", config.stream_port);
    printf("  Passthrough Test: %s\n", config.passthrough_test ? "Enabled" : "Disabled");
    printf("  Oneshot Mode: %s\n", config.oneshot_mode ? "Enabled" : "Disabled");
    printf("  Buffer Size: %d\n", config.buffer_size);

    char latency[32];
    snprintf(latency, sizeof(latency), "%d/48000", config.buffer_size);
    setenv("PIPEWIRE_LATENCY", latency, 1);

    int ret = pwar_cli_run(&config);
    return ret;
}
