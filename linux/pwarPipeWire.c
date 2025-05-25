 #include <stdio.h>
 #include <errno.h>
 #include <math.h>
 #include <signal.h>

#include <errno.h>
#include <math.h>
#include <signal.h>
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

 #include "pwar_packet.h"

#define STREAM_IP "10.0.0.161"
#define STREAM_PORT 8321
  
 struct data;
  
 struct port {
         struct data *data;
 };
  
 struct data {
         struct pw_main_loop *loop;
         struct pw_filter *filter;
         struct port *in_port;
         struct port *out_port;
         float sine_phase;
         uint8_t test_mode;

         uint32_t seq;
         int sockfd;
         struct sockaddr_in servaddr;
 };
  
 void setup_socket(void *userdata)
 {
    struct data *data = userdata;
     // Create a socket
     data->sockfd = socket(AF_INET, SOCK_DGRAM, 0);
     if (data->sockfd < 0)
     {
         perror("socket creation failed");
         exit(EXIT_FAILURE);
     }
 
     // Set up the server address
     memset(&data->servaddr, 0, sizeof(data->servaddr));
     data->servaddr.sin_family = AF_INET;
     data->servaddr.sin_port = htons(STREAM_PORT);
     data->servaddr.sin_addr.s_addr = inet_addr(STREAM_IP);
 }

static void stream_buffer(float *samples, uint32_t n_samples, void *userdata)
{
    struct data *data = userdata;
    rt_stream_packet_t packet;

    packet.seq = data->seq;
    data->seq += 1;

    packet.n_samples = n_samples;
    memcpy(packet.samples, samples, n_samples * sizeof(float));

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t timestamp = ts.tv_sec * 1000000000 + ts.tv_nsec;

    packet.timestamp = timestamp;

    // Send the packet
    if (sendto(data->sockfd, &packet, sizeof(packet), 0, (struct sockaddr *)&data->servaddr, sizeof(data->servaddr)) < 0)
    {
        perror("sendto failed");
    }
}

 /* our data processing function is in general:
  *
  *  struct pw_buffer *b;
  *  in = pw_filter_dequeue_buffer(filter, in_port);
  *  out = pw_filter_dequeue_buffer(filter, out_port);
  *
  *  .. do stuff with buffers ...
  *
  *  pw_filter_queue_buffer(filter, in_port, in);
  *  pw_filter_queue_buffer(filter, out_port, out);
  *
  *  For DSP ports, there is a shortcut to directly dequeue, get
  *  the data and requeue the buffer with pw_filter_get_dsp_buffer().
  *
  *
  */
 // TODO Move the pwar processing here
 static void on_process(void *userdata, struct spa_io_position *position)
 {
         struct data *data = userdata;
         float *in, *out;
         uint32_t n_samples = position->clock.duration;
         printf("on_process: %p, n_samples: %d\n", data, n_samples);
  
         pw_log_trace("do process %d", n_samples);
  
         in = pw_filter_get_dsp_buffer(data->in_port, n_samples);
         out = pw_filter_get_dsp_buffer(data->out_port, n_samples);

         if (data->test_mode)
         {
            for (uint32_t n = 0; n < n_samples; n++)
            {
                if (data->sine_phase >= 2 * M_PI)
                    data->sine_phase -= 2 * M_PI;
                in[n] = sinf(data->sine_phase) * 0.5f;
                data->sine_phase += 2 * M_PI * 440 / 48000; // 440 Hz sine wave
            }
         }

         stream_buffer(in, n_samples, data);
  
         if (in == NULL || out == NULL)
                 return;
  
         memcpy(out, in, n_samples * sizeof(float));
 }
  
 static const struct pw_filter_events filter_events = {
         PW_VERSION_FILTER_EVENTS,
         .process = on_process,
 };
  
 static void do_quit(void *userdata, int signal_number)
 {
         struct data *data = userdata;
         pw_main_loop_quit(data->loop);
 }
  
 int main(int argc, char *argv[])
 {
        char latency[20];
        sprintf(latency, "%d/48000", 256);
        setenv("PIPEWIRE_LATENCY", latency, 1);


         struct data data = { 0, };
         const struct spa_pod *params[1];
         uint8_t buffer[1024];
         struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

         setup_socket(&data);
  
         pw_init(&argc, &argv);
  
         /* make a main loop. If you already have another main loop, you can add
          * the fd of this pipewire mainloop to it. */
         data.loop = pw_main_loop_new(NULL);
  
         pw_loop_add_signal(pw_main_loop_get_loop(data.loop), SIGINT, do_quit, &data);
         pw_loop_add_signal(pw_main_loop_get_loop(data.loop), SIGTERM, do_quit, &data);

         data.test_mode = 1; // Enable test mode for sine wave generation
         data.sine_phase = 0.0f; // Initialize sine phase
  
         /* Create a simple filter, the simple filter manages the core and remote
          * objects for you if you don't need to deal with them.
          *
          * Pass your events and a user_data pointer as the last arguments. This
          * will inform you about the filter state. The most important event
          * you need to listen to is the process event where you need to process
          * the data.
          */
         data.filter = pw_filter_new_simple(
                         pw_main_loop_get_loop(data.loop),
                         "audio-filter",
                         pw_properties_new(
                                 PW_KEY_MEDIA_TYPE, "Audio",
                                 PW_KEY_MEDIA_CATEGORY, "Filter",
                                 PW_KEY_MEDIA_ROLE, "DSP",
                                 NULL),
                         &filter_events,
                         &data);
  
         /* make an audio DSP input port */
         data.in_port = pw_filter_add_port(data.filter,
                         PW_DIRECTION_INPUT,
                         PW_FILTER_PORT_FLAG_MAP_BUFFERS,
                         sizeof(struct port),
                         pw_properties_new(
                                 PW_KEY_FORMAT_DSP, "32 bit float mono audio",
                                 PW_KEY_PORT_NAME, "input",
                                 NULL),
                         NULL, 0);
  
         /* make an audio DSP output port */
         data.out_port = pw_filter_add_port(data.filter,
                         PW_DIRECTION_OUTPUT,
                         PW_FILTER_PORT_FLAG_MAP_BUFFERS,
                         sizeof(struct port),
                         pw_properties_new(
                                 PW_KEY_FORMAT_DSP, "32 bit float mono audio",
                                 PW_KEY_PORT_NAME, "output",
                                 NULL),
                         NULL, 0);
  
         params[0] = spa_process_latency_build(&b,
                         SPA_PARAM_ProcessLatency,
                         &SPA_PROCESS_LATENCY_INFO_INIT(
                                 .ns = 10 * SPA_NSEC_PER_MSEC
                         ));
  
  
         /* Now connect this filter. We ask that our process function is
          * called in a realtime thread. */
         if (pw_filter_connect(data.filter,
                                 PW_FILTER_FLAG_RT_PROCESS,
                                 params, 1) < 0) {
                 fprintf(stderr, "can't connect\n");
                 return -1;
         }
  
         /* and wait while we let things run */
         pw_main_loop_run(data.loop);
  
         pw_filter_destroy(data.filter);
         pw_main_loop_destroy(data.loop);
         pw_deinit();
  
         return 0;
 }
