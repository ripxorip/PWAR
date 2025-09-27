// PWAR CLI - Unified CLI application supporting both ALSA and PipeWire backends
// Uses the new unified PWAR architecture
// Compile: gcc -O2 -o pwar_cli_new pwar_cli_new.c libpwar_new.c alsa_backend.c pipewire_backend.c audio_backend.c -lasound -lpipewire-0.3 -lspa-0.2 -lm -lpthread

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "libpwar.h"

// Default configuration
#define DEFAULT_STREAM_IP          "192.168.66.3"
#define DEFAULT_STREAM_PORT        8321
#define DEFAULT_PASSTHROUGH_TEST   0        // 1 = local passthrough test
#define DEFAULT_DEVICE_BUFFER_SIZE 32       // Device buffer size in frames
#define DEFAULT_WINDOWS_PACKET_SIZE 64      // Windows packet buffer size in frames
#define DEFAULT_RING_BUFFER_DEPTH  2048     // Ring buffer depth in samples

// Audio defaults
#define DEFAULT_SAMPLE_RATE         48000
#define DEFAULT_FRAMES              32      // Legacy - same as device buffer size
#define DEFAULT_CHANNELS            2

// ALSA specific defaults
#define DEFAULT_PCM_DEVICE_PLAYBACK "hw:3,0"
#define DEFAULT_PCM_DEVICE_CAPTURE  "hw:3,0"

static void print_usage(const char *program_name) {
    printf("Usage: %s [options]\n", program_name);
    printf("Options:\n");
    printf("  --backend <backend>        Audio backend: alsa or pipewire (default: pipewire)\n");
    printf("  -i, --ip <ip>              Target IP address (default: %s)\n", DEFAULT_STREAM_IP);
    printf("  --port <port>              Target port (default: %d)\n", DEFAULT_STREAM_PORT);
    printf("  -t, --passthrough          Enable passthrough test mode\n");
    printf("  -b, --device-buffer <size> Device buffer size in frames (default: %d)\n", DEFAULT_DEVICE_BUFFER_SIZE);
    printf("  -p, --packet-buffer <size> Windows packet buffer size in frames (default: %d)\n", DEFAULT_WINDOWS_PACKET_SIZE);
    printf("  -r, --rate <rate>          Sample rate (default: %d)\n", DEFAULT_SAMPLE_RATE);
    printf("  -d, --ring-depth <depth>   Ring buffer depth in samples (default: %d)\n", DEFAULT_RING_BUFFER_DEPTH);
    printf("  --capture-device <device>  ALSA capture device (ALSA only, default: %s)\n", DEFAULT_PCM_DEVICE_CAPTURE);
    printf("  --playback-device <device> ALSA playback device (ALSA only, default: %s)\n", DEFAULT_PCM_DEVICE_PLAYBACK);
    printf("  -h, --help                 Show this help message\n");
    printf("\nBuffer size guidelines:\n");
    printf("  Device buffer: 32, 64, 128, 256 frames (lower = lower latency, higher CPU load)\n");
    printf("  Packet buffer: Must be multiple of device buffer (64, 128, 256, 512 frames)\n");
    printf("\nBackends:\n");
    printf("  alsa                       Use ALSA for audio I/O\n");
    printf("  pipewire                   Use PipeWire for audio I/O\n");
    printf("  simulated                  Use simulated audio for testing (no hardware needed)\n");
    printf("\nExamples:\n");
    printf("  %s                         # Use PipeWire with default settings\n", program_name);
    printf("  %s --backend alsa -i 192.168.1.100 --port 9000 -b 64 -p 128\n", program_name);
    printf("  %s --backend pipewire -b 32 -p 64\n", program_name);
    printf("  %s --backend simulated --passthrough   # Test mode without hardware\n", program_name);
}

static audio_backend_type_t parse_backend(const char *backend_str) {
    if (strcmp(backend_str, "alsa") == 0) {
        return AUDIO_BACKEND_ALSA;
    } else if (strcmp(backend_str, "pipewire") == 0) {
        return AUDIO_BACKEND_PIPEWIRE;
    } else if (strcmp(backend_str, "simulated") == 0) {
        return AUDIO_BACKEND_SIMULATED;
    } else {
        return AUDIO_BACKEND_PIPEWIRE; // Default fallback
    }
}

static int parse_arguments(int argc, char *argv[], pwar_config_t *config) {
    // Set defaults
    strcpy(config->stream_ip, DEFAULT_STREAM_IP);
    config->stream_port = DEFAULT_STREAM_PORT;
    config->passthrough_test = DEFAULT_PASSTHROUGH_TEST;
    config->device_buffer_size = DEFAULT_DEVICE_BUFFER_SIZE;
    config->windows_packet_size = DEFAULT_WINDOWS_PACKET_SIZE;
    config->ring_buffer_depth = DEFAULT_RING_BUFFER_DEPTH;
    config->backend_type = AUDIO_BACKEND_PIPEWIRE; // Default to PipeWire
    
    // Audio config defaults
    config->audio_config.device_playback = DEFAULT_PCM_DEVICE_PLAYBACK;
    config->audio_config.device_capture = DEFAULT_PCM_DEVICE_CAPTURE;
    config->audio_config.sample_rate = DEFAULT_SAMPLE_RATE;
    config->audio_config.frames = DEFAULT_DEVICE_BUFFER_SIZE;  // Use device buffer size
    config->audio_config.playback_channels = DEFAULT_CHANNELS;
    config->audio_config.capture_channels = DEFAULT_CHANNELS;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return -1;
        } else if (strcmp(argv[i], "--backend") == 0 && i + 1 < argc) {
            config->backend_type = parse_backend(argv[++i]);
        } else if ((strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--ip") == 0) && i + 1 < argc) {
            strcpy(config->stream_ip, argv[++i]);
        } else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            config->stream_port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--passthrough") == 0) {
            config->passthrough_test = 1;
        } else if ((strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--device-buffer") == 0) && i + 1 < argc) {
            int device_buffer = atoi(argv[++i]);
            config->device_buffer_size = device_buffer;
            config->audio_config.frames = device_buffer;  // Keep audio config in sync
        } else if ((strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--packet-buffer") == 0) && i + 1 < argc) {
            config->windows_packet_size = atoi(argv[++i]);
        } else if ((strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--rate") == 0) && i + 1 < argc) {
            config->audio_config.sample_rate = atoi(argv[++i]);
        } else if ((strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--ring-depth") == 0) && i + 1 < argc) {
            config->ring_buffer_depth = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--capture-device") == 0 && i + 1 < argc) {
            config->audio_config.device_capture = argv[++i];
        } else if (strcmp(argv[i], "--playback-device") == 0 && i + 1 < argc) {
            config->audio_config.device_playback = argv[++i];
        } else {
            fprintf(stderr, "Unknown argument: %s\n", argv[i]);
            print_usage(argv[0]);
            return -1;
        }
    }
    
    // Validate that windows_packet_size is a multiple of device_buffer_size
    if (config->windows_packet_size % config->device_buffer_size != 0) {
        fprintf(stderr, "Error: Windows packet buffer size (%d) must be a multiple of device buffer size (%d)\n",
                config->windows_packet_size, config->device_buffer_size);
        return -1;
    }
    
    return 0;
}

int main(int argc, char *argv[]) {
    pwar_config_t config;
    
    printf("PWAR CLI - Low-latency audio streaming with PWAR protocol\n");
    printf("Unified architecture supporting multiple audio backends\n\n");
    
    // Parse command line arguments
    if (parse_arguments(argc, argv, &config) < 0) {
        return 1;
    }
    
    // Validate backend availability
    if (!audio_backend_is_available(config.backend_type)) {
        const char *backend_name = "Unknown";
        if (config.backend_type == AUDIO_BACKEND_ALSA) backend_name = "ALSA";
        else if (config.backend_type == AUDIO_BACKEND_PIPEWIRE) backend_name = "PipeWire";
        else if (config.backend_type == AUDIO_BACKEND_SIMULATED) backend_name = "Simulated";
        
        printf("Error: %s backend is not available (not compiled in)\n", backend_name);
        printf("Available backends:\n");
        if (audio_backend_is_available(AUDIO_BACKEND_ALSA)) {
            printf("  - ALSA\n");
        }
        if (audio_backend_is_available(AUDIO_BACKEND_PIPEWIRE)) {
            printf("  - PipeWire\n");
        }
        if (audio_backend_is_available(AUDIO_BACKEND_SIMULATED)) {
            printf("  - Simulated\n");
        }
        return 1;
    }
    
    // Print configuration
    printf("Configuration:\n");
    printf("  Target: %s:%d\n", config.stream_ip, config.stream_port);
    printf("  Passthrough test: %s\n", config.passthrough_test ? "enabled" : "disabled");
    
    const char *backend_name = "Unknown";
    if (config.backend_type == AUDIO_BACKEND_ALSA) backend_name = "ALSA";
    else if (config.backend_type == AUDIO_BACKEND_PIPEWIRE) backend_name = "PipeWire";
    else if (config.backend_type == AUDIO_BACKEND_SIMULATED) backend_name = "Simulated";
    printf("  Backend: %s\n", backend_name);
    
    printf("  Sample rate: %u Hz\n", config.audio_config.sample_rate);
    printf("  Device buffer size: %u frames (%.2f ms)\n", 
           config.device_buffer_size,
           (double)config.device_buffer_size * 1000.0 / config.audio_config.sample_rate);
    printf("  Windows packet size: %u frames (%.2f ms)\n", 
           config.windows_packet_size,
           (double)config.windows_packet_size * 1000.0 / config.audio_config.sample_rate);
    printf("  Packets per send: %u device buffers\n",
           config.windows_packet_size / config.device_buffer_size);
    printf("  Ring buffer depth: %d samples (%.2f ms)\n", 
           config.ring_buffer_depth,
           (double)config.ring_buffer_depth * 1000.0 / config.audio_config.sample_rate);
    
    if (config.backend_type == AUDIO_BACKEND_ALSA) {
        printf("  Capture device: %s (%u channels)\n", 
               config.audio_config.device_capture, config.audio_config.capture_channels);
        printf("  Playback device: %s (%u channels)\n", 
               config.audio_config.device_playback, config.audio_config.playback_channels);
    } else if (config.backend_type == AUDIO_BACKEND_PIPEWIRE) {
        printf("  Audio I/O: PipeWire filter with %u channels\n", config.audio_config.capture_channels);
    } else if (config.backend_type == AUDIO_BACKEND_SIMULATED) {
        printf("  Audio I/O: Simulated audio\n"); 
               
    }
    printf("\n");
    
    // Run PWAR CLI
    int result = pwar_cli_run(&config);
    
    if (result < 0) {
        fprintf(stderr, "PWAR CLI failed to start\n");
        return 1;
    }
    
    printf("PWAR CLI finished successfully\n");
    return 0;
}
