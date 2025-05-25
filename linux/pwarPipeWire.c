#include <stdio.h>
#include <errno.h>
#include <math.h>
#include <signal.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <unistd.h>

#include <spa/param/audio/format-utils.h>

#include <pipewire/pipewire.h>

#include "pwar_packet.h"

//#define STREAM_IP "127.0.0.1"
#define STREAM_IP "10.0.0.161"
#define STREAM_PORT 8321

#define SAMPLE_RATE 48000

#define TEST_MODE 1

struct data
{
    struct pw_main_loop *loop;
    struct pw_stream *stream;

    struct spa_audio_info format;
    unsigned move : 1;
};

struct
{
    uint32_t seq;
    int sockfd;
    struct sockaddr_in servaddr;
    int test_mode;
    int sine_active;
    float sine_phase;
} internal = {0};

void setup_socket()
{
    // Create a socket
    internal.sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (internal.sockfd < 0)
    {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set up the server address
    memset(&internal.servaddr, 0, sizeof(internal.servaddr));
    internal.servaddr.sin_family = AF_INET;
    internal.servaddr.sin_port = htons(STREAM_PORT);
    internal.servaddr.sin_addr.s_addr = inet_addr(STREAM_IP);
}

static void stream_buffer(float *samples, uint32_t n_samples)
{
    rt_stream_packet_t packet;

    packet.seq = internal.seq;
    internal.seq += 1;

    packet.n_samples = n_samples;
    memcpy(packet.samples, samples, n_samples * sizeof(float));

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t timestamp = ts.tv_sec * 1000000000 + ts.tv_nsec;

    packet.timestamp = timestamp;

    // Send the packet
    if (sendto(internal.sockfd, &packet, sizeof(packet), 0, (struct sockaddr *)&internal.servaddr, sizeof(internal.servaddr)) < 0)
    {
        perror("sendto failed");
    }
}

static void* listen_for_input(void *arg)
{
    char buffer[1024];
    int n;

    while (1)
    {
        n = read(0, buffer, 1024);
        (void)n;
        internal.sine_active = !internal.sine_active;
    }

    return NULL;
}

/* our data processing function is in general:
 *
 *  struct pw_buffer *b;
 *  b = pw_stream_dequeue_buffer(stream);
 *
 *  .. consume stuff in the buffer ...
 *
 *  pw_stream_queue_buffer(stream, b);
 */
static void on_process(void *userdata)
{
    struct data *data = userdata;
    struct pw_buffer *b;
    struct spa_buffer *buf;
    float *samples, max;
    uint32_t c, n, n_channels, n_samples, peak;

    static int count = 0;

    static float fbuf[RT_STREAM_PACKET_FRAME_SIZE];

    if ((b = pw_stream_dequeue_buffer(data->stream)) == NULL)
    {
        pw_log_warn("out of buffers: %m");
        return;
    }

    buf = b->buffer;
    if ((samples = buf->datas[0].data) == NULL)
        return;

    n_channels = data->format.info.raw.channels;
    n_samples = buf->datas[0].chunk->size / sizeof(float);

    if (internal.test_mode)
    {
        if (internal.sine_active) {
            for (n = 0; n < n_samples; n++)
            {
                samples[n] = sinf(internal.sine_phase) * 0.5;
                internal.sine_phase += 2 * M_PI * 440 / SAMPLE_RATE;
                if (internal.sine_phase >= 2 * M_PI)
                {
                    internal.sine_phase -= 2 * M_PI;
                }
            }
        }
        else
        {
            for (n = 0; n < n_samples; n++)
            {
                samples[n] = 0.0f;
            }
        }
    }

    /* move cursor up */
    if (data->move)
        fprintf(stdout, "%c[%dA", 0x1b, n_channels + 1);
    fprintf(stdout, "captured %d samples\n", n_samples / n_channels);
    for (c = 0; c < data->format.info.raw.channels; c++)
    {
        max = 0.0f;
        for (n = c; n < n_samples; n += n_channels)
        {
            max = fmaxf(max, fabsf(samples[n]));
            if (c == 0)
            {
                fbuf[count] = samples[n];
                count += 1;
            }
        }

        peak = SPA_CLAMP(max * 30, 0, 39);

        fprintf(stdout, "channel %d: |%*s%*s| peak:%f\n",
                c, peak + 1, "*", 40 - peak, "", max);
    }
    data->move = true;
    fflush(stdout);

    if (count == RT_STREAM_PACKET_FRAME_SIZE)
    {
        stream_buffer(fbuf, RT_STREAM_PACKET_FRAME_SIZE);
        count = 0;
    }


    pw_stream_queue_buffer(data->stream, b);
}

/* Be notified when the stream param changes. We're only looking at the
 * format changes.
 */
static void
on_stream_param_changed(void *_data, uint32_t id, const struct spa_pod *param)
{
    struct data *data = _data;

    /* NULL means to clear the format */
    if (param == NULL || id != SPA_PARAM_Format)
        return;

    if (spa_format_parse(param, &data->format.media_type, &data->format.media_subtype) < 0)
        return;

    /* only accept raw audio */
    if (data->format.media_type != SPA_MEDIA_TYPE_audio ||
        data->format.media_subtype != SPA_MEDIA_SUBTYPE_raw)
        return;

    /* call a helper function to parse the format for us. */
    spa_format_audio_raw_parse(param, &data->format.info.raw);

    fprintf(stdout, "capturing rate:%d channels:%d\n",
            data->format.info.raw.rate, data->format.info.raw.channels);
}

static const struct pw_stream_events stream_events = {
    PW_VERSION_STREAM_EVENTS,
    .param_changed = on_stream_param_changed,
    .process = on_process,
};

static void do_quit(void *userdata, int signal_number)
{
    (void)signal_number;
    struct data *data = userdata;
    pw_main_loop_quit(data->loop);
}

int main(int argc, char *argv[])
{
    internal.test_mode = TEST_MODE;

    if (internal.test_mode)
    {
        printf("Test mode activated\n");
        pthread_t input_thread;
        pthread_create(&input_thread, NULL, listen_for_input, NULL);
    }

    struct data data = {
        0,
    };
    const struct spa_pod *params[1];
    uint8_t buffer[1024];
    struct pw_properties *props;
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

    setup_socket();

    char latency[20];
    sprintf(latency, "%d/48000", RT_STREAM_PACKET_FRAME_SIZE);
    setenv("PIPEWIRE_LATENCY", latency, 1);

    pw_init(&argc, &argv);

    /* make a main loop. If you already have another main loop, you can add
     * the fd of this pipewire mainloop to it. */
    data.loop = pw_main_loop_new(NULL);

    pw_loop_add_signal(pw_main_loop_get_loop(data.loop), SIGINT, do_quit, &data);
    pw_loop_add_signal(pw_main_loop_get_loop(data.loop), SIGTERM, do_quit, &data);

    /* Create a simple stream, the simple stream manages the core and remote
     * objects for you if you don't need to deal with them.
     *
     * If you plan to autoconnect your stream, you need to provide at least
     * media, category and role properties.
     *
     * Pass your events and a user_data pointer as the last arguments. This
     * will inform you about the stream state. The most important event
     * you need to listen to is the process event where you need to produce
     * the data.
     */
    props = pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio",
                              PW_KEY_CONFIG_NAME, "client-rt.conf",
                              PW_KEY_MEDIA_CATEGORY, "Capture",
                              PW_KEY_MEDIA_ROLE, "Music",
                              NULL);
    if (argc > 1)
        /* Set stream target if given on command line */
        pw_properties_set(props, PW_KEY_TARGET_OBJECT, argv[1]);

    /* uncomment if you want to capture from the sink monitor ports */
    /* pw_properties_set(props, PW_KEY_STREAM_CAPTURE_SINK, "true"); */

    data.stream = pw_stream_new_simple(
        pw_main_loop_get_loop(data.loop),
        "audio-capture",
        props,
        &stream_events,
        &data);

    /* Make one parameter with the supported formats. The SPA_PARAM_EnumFormat
     * id means that this is a format enumeration (of 1 value).
     * We leave the channels and rate empty to accept the native graph
     * rate and channels. */
    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat,
                                           &SPA_AUDIO_INFO_RAW_INIT(
                                                   .rate = SAMPLE_RATE,
                                                   .channels = 1,
                                                   .format = SPA_AUDIO_FORMAT_F32));

    /* Now connect this stream. We ask that our process function is
     * called in a realtime thread. */
    pw_stream_connect(data.stream,
                      PW_DIRECTION_INPUT,
                      PW_ID_ANY,
                      PW_STREAM_FLAG_AUTOCONNECT |
                          PW_STREAM_FLAG_MAP_BUFFERS |
                          PW_STREAM_FLAG_RT_PROCESS,
                      params, 1);

    /* and wait while we let things run */
    pw_main_loop_run(data.loop);

    pw_stream_destroy(data.stream);
    pw_main_loop_destroy(data.loop);
    pw_deinit();

    return 0;
}
