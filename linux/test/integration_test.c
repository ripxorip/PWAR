/*
 * integration_test.c - Minimal PipeWire passthrough/sine generator for integration testing
 *
 * (c) 2025 Philip K. Gisslow
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <pipewire/pipewire.h>
#include <pipewire/filter.h>
#include <spa/pod/builder.h>
#include <spa/param/latency-utils.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/wait.h>

struct data;

struct port {
    struct data *data;
};

struct data {
    struct pw_main_loop *loop;
    struct pw_filter *filter;
    void *in_port;
    void *in_port2;
    void *dummy_in_port;
    void *out_port;
    void *test_out_left;
    void *test_out_right;
    float sine_phase;
    uint8_t test_mode;
    pid_t pid_pipewire;
    pid_t pid_windows_sim;
    // Pattern injection/detection fields
    int inject_pattern;
    uint32_t inject_sample_idx;
    uint32_t detect_sample_idx_in;
    uint32_t detect_sample_idx_in2;
    int pattern_detected_in;
    int pattern_detected_in2;
    uint64_t global_sample_idx; // Absolute sample counter
};

static void on_process(void *userdata, struct spa_io_position *position) {
    struct data *data = (struct data *)userdata;
    float *in = pw_filter_get_dsp_buffer(data->in_port, position->clock.duration);
    float *in2 = pw_filter_get_dsp_buffer(data->in_port2, position->clock.duration);
    float *out = pw_filter_get_dsp_buffer(data->out_port, position->clock.duration);
    float *test_out_left = pw_filter_get_dsp_buffer(data->test_out_left, position->clock.duration);
    float *test_out_right = pw_filter_get_dsp_buffer(data->test_out_right, position->clock.duration);

    uint32_t n_samples = position->clock.duration;
    static const float pattern = 0.999f; // Unique value unlikely in sine
    for (uint32_t n = 0; n < n_samples; n++) {
        if (data->sine_phase >= 2 * M_PI)
            data->sine_phase -= 2 * M_PI;
        float val1 = sinf(data->sine_phase) * 0.5f;
        data->sine_phase += 2.0f * M_PI * 440.0f / 48000.0f; // Increment phase for 440Hz sine at 48kHz

        // Inject pattern if requested
        if (data->inject_pattern) {
            if (out) out[n] = pattern;
            data->inject_pattern = 0;
            data->inject_sample_idx = data->global_sample_idx;
        } else {
            if (out) out[n] = val1;
        }

        // Detect pattern in in/in2
        if (in && !data->pattern_detected_in && fabsf(in[n] - pattern) < 1e-5) {
            data->detect_sample_idx_in = data->global_sample_idx;
            data->pattern_detected_in = 1;
            // Print the delay as ms (48khz sample rate)
            printf("[integration_test] Pattern detected in input delay: %.2f ms\n", (data->global_sample_idx - data->inject_sample_idx) * 1000.0f / 48000.0f);
        }

        if (test_out_left && in) test_out_left[n] = in[n];
        if (test_out_right && in2) test_out_right[n] = in2[n];
        data->global_sample_idx++; // Increment global sample counter
    }
}

static const struct pw_filter_events filter_events = {
    PW_VERSION_FILTER_EVENTS,
    .process = on_process,
};

static void do_quit(void *userdata, int signal_number) {
    struct data *data = (struct data *)userdata;
    int status;
    // Kill child processes if running
    if (data->pid_pipewire > 0) {
        kill(data->pid_pipewire, SIGTERM);
        // Wait up to 1 second for process to exit
        for (int i = 0; i < 10; ++i) {
            if (waitpid(data->pid_pipewire, &status, WNOHANG) > 0) break;
            usleep(100000);
        }
        // If still running, force kill
        if (waitpid(data->pid_pipewire, &status, WNOHANG) == 0) {
            kill(data->pid_pipewire, SIGKILL);
            waitpid(data->pid_pipewire, &status, 0);
        }
    }
    if (data->pid_windows_sim > 0) {
        printf("Killing windows_sim process with PID %d\n", data->pid_windows_sim);
        kill(data->pid_windows_sim, SIGTERM);
        for (int i = 0; i < 10; ++i) {
            if (waitpid(data->pid_windows_sim, &status, WNOHANG) > 0) break;
            usleep(100000);
        }
        if (waitpid(data->pid_windows_sim, &status, WNOHANG) == 0) {
            kill(data->pid_windows_sim, SIGKILL);
            waitpid(data->pid_windows_sim, &status, 0);
        }
    }
    pw_main_loop_quit(data->loop);
}

void *test_thread_func(void *arg) {
    struct data *data = (struct data *)arg;
    sleep(1);
    printf("[integration_test] Test thread running. You can add PipeWire graph manipulation, start other programs, etc.\n");
    int ret = system("pw-link alsa_input.pci-0000_00_1f.3.analog-stereo:capture_FL integration_test:dummy_in");
    (void)ret;

    data->pid_pipewire = fork();
    if (data->pid_pipewire == 0) {
        // Child process: exec pwar with arguments
        //execl("build/pwar_cli", "pwar_cli", "--ip", "127.0.0.1", "--port", "8322", "--oneshot", "--buffer_size", "64", (char *)NULL);
        execl("build/pwar_cli", "pwar_cli", "--ip", "127.0.0.1", "--port", "8322", "--buffer_size", "128", (char *)NULL);
        perror("execl pwar");
        exit(1);
    } else if (data->pid_pipewire < 0) {
        perror("fork for pwar");
    }

    data->pid_windows_sim = fork();
    if (data->pid_windows_sim == 0) {
        // Child process: exec windows_sim
        execl("build/windows_sim", "windows_sim", (char *)NULL);
        perror("execl windows_sim");
        exit(1);
    } else if (data->pid_windows_sim < 0) {
        perror("fork for windows_sim");
    }

    // Parent: wire everything with pw-link
    // Wait a moment for children to start and register their nodes
    sleep(2);

    ret = system("pw-link integration_test:output pwar:input ");
    ret = system("pw-link pwar:output-right integration_test:input-right");
    ret = system("pw-link pwar:output-left integration_test:input-left");

    ret = system("pw-link integration_test:test-out-left alsa_output.pci-0000_00_1f.3.analog-stereo:playback_FL");
    ret = system("pw-link integration_test:test-out-right alsa_output.pci-0000_00_1f.3.analog-stereo:playback_FR");

    // Key press detection loop
    printf("[integration_test] Press RET to inject pattern into sine stream.\n");
    while (1) {
        // Non-blocking key press detection
        fd_set set;
        struct timeval timeout;
        FD_ZERO(&set);
        FD_SET(0, &set); // stdin
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000; // 100ms
        int rv = select(1, &set, NULL, NULL, &timeout);
        if (rv > 0) {
            int c = getchar();
            (void)c;
            data->inject_pattern = 1;
            data->pattern_detected_in = 0;
            data->pattern_detected_in2 = 0;
            printf("[integration_test] RET pressed, will inject pattern.\n");
        }
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    struct data data;
    memset(&data, 0, sizeof(data));
    data.test_mode = 1;
    data.sine_phase = 0.0f;
    data.global_sample_idx = 0;

    pw_init(&argc, &argv);
    data.loop = pw_main_loop_new(NULL);
    pw_loop_add_signal(pw_main_loop_get_loop(data.loop), SIGINT, do_quit, &data);
    pw_loop_add_signal(pw_main_loop_get_loop(data.loop), SIGTERM, do_quit, &data);

    // Start test thread before running main loop
    pthread_t test_thread;
    pthread_create(&test_thread, NULL, test_thread_func, &data);

    data.filter = pw_filter_new_simple(
        pw_main_loop_get_loop(data.loop),
        "integration_test",
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
            PW_KEY_PORT_NAME, "input-left",
            NULL),
        NULL, 0);
    data.in_port2 = pw_filter_add_port(data.filter,
        PW_DIRECTION_INPUT,
        PW_FILTER_PORT_FLAG_MAP_BUFFERS,
        sizeof(struct port),
        pw_properties_new(
            PW_KEY_FORMAT_DSP, "32 bit float mono audio",
            PW_KEY_PORT_NAME, "input-right",
            NULL),
        NULL, 0);
    data.dummy_in_port = pw_filter_add_port(data.filter,
        PW_DIRECTION_INPUT,
        PW_FILTER_PORT_FLAG_MAP_BUFFERS,
        sizeof(struct port),
        pw_properties_new(
            PW_KEY_FORMAT_DSP, "32 bit float mono audio",
            PW_KEY_PORT_NAME, "dummy_in",
            NULL),
        NULL, 0);
    data.out_port = pw_filter_add_port(data.filter,
        PW_DIRECTION_OUTPUT,
        PW_FILTER_PORT_FLAG_MAP_BUFFERS,
        sizeof(struct port),
        pw_properties_new(
            PW_KEY_FORMAT_DSP, "32 bit float mono audio",
            PW_KEY_PORT_NAME, "output",
            NULL),
        NULL, 0);
    data.test_out_left = pw_filter_add_port(data.filter,
        PW_DIRECTION_OUTPUT,
        PW_FILTER_PORT_FLAG_MAP_BUFFERS,
        sizeof(struct port),
        pw_properties_new(
            PW_KEY_FORMAT_DSP, "32 bit float mono audio",
            PW_KEY_PORT_NAME, "test-out-left",
            NULL),
        NULL, 0);
    data.test_out_right = pw_filter_add_port(data.filter,
        PW_DIRECTION_OUTPUT,
        PW_FILTER_PORT_FLAG_MAP_BUFFERS,
        sizeof(struct port),
        pw_properties_new(
            PW_KEY_FORMAT_DSP, "32 bit float mono audio",
            PW_KEY_PORT_NAME, "test-out-right",
            NULL),
        NULL, 0);

    uint8_t buffer[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    const struct spa_pod *params[1];
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
