#include <check.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#include "../pwar_send_buffer.h"
#include "../pwar_router.h"
#include "../pwar_rcv_buffer.h"

#define CHANNELS 2
#define CHUNK_SIZE 128
#define WIN_BUFFER 1024

// Helper to set up test sine wave arrays and fill them
static void fill_test_sine_wave(float *test_samples, float **test_samples_ptrs, uint32_t channels, uint32_t n_test_samples, float frequency, float sample_rate) {
    for (uint32_t ch = 0; ch < channels; ++ch) {
        test_samples_ptrs[ch] = test_samples + ch * n_test_samples;
        for (uint32_t s = 0; s < n_test_samples; ++s) {
            test_samples[ch * n_test_samples + s] = sinf(2 * M_PI * frequency * s / sample_rate);
        }
    }
}

START_TEST(test_send_and_rcv)
{
    const uint32_t n_test_samples = 8192;
    pwar_router_t linux_router;
    pwar_router_t win_router;

    pwar_router_init(&win_router, CHANNELS);

    // Generate a test sine wave, 2 channels, 8192 samples, 48 kHz sample rate, 440 Hz frequency
    float test_samples[CHANNELS][n_test_samples];
    float *test_samples_ptrs[CHANNELS];
    fill_test_sine_wave(&test_samples[0][0], test_samples_ptrs, CHANNELS, n_test_samples, 440.0f, 48000.0f);

    float result_samples[CHANNELS][n_test_samples];

    // Initialize Linux side
    pwar_router_init(&linux_router, CHANNELS);
    pwar_send_buffer_init(CHANNELS, WIN_BUFFER);
    // The rcv buffer doesnt need any initialization, it is a singleton API

    // Initialize Windows side
    pwar_router_init(&win_router, CHANNELS);

    // Loop over the test_samples in chunks
    for (uint32_t start = 0; start < n_test_samples; start += CHUNK_SIZE) {
        // Linux side:
        float *chunk_ptrs[CHANNELS];
        for (uint32_t ch = 0; ch < CHANNELS; ++ch) {
            chunk_ptrs[ch] = &test_samples[ch][start];
        }
        pwar_send_buffer_push(chunk_ptrs, CHUNK_SIZE);

        if (pwar_send_buffer_ready()) {
            float *linux_send_samples[CHANNELS];
            uint32_t n_samples = 0;
            pwar_send_buffer_get(linux_send_samples, &n_samples);

            pwar_packet_t packets[32] = {0};
            uint32_t packets_to_send = 0;
            pwar_router_send_buffer(&linux_router, linux_send_samples, n_samples, CHANNELS, packets, 32, &packets_to_send);
            // The packets are now in the packets array, ready to be sent

            // NETWORK DELAY

            // (Fake sending the packets to Windows side)
            // Windows side (receiving):
            for (uint32_t p = 0; p < packets_to_send; ++p) {
                // Windows output buffer
                float win_output_buffers[CHANNELS][WIN_BUFFER] = {0};
                float *win_output_ptrs[CHANNELS];
                for (uint32_t ch = 0; ch < CHANNELS; ++ch) {
                    win_output_ptrs[ch] = win_output_buffers[ch];
                }
                int r = pwar_router_process_packet(&win_router, &packets[p], win_output_ptrs, WIN_BUFFER, CHANNELS);

                if (r == 1) {
                    // FAKE PROCESSING..
                    // Packet ready and we have output to send to Linux side
                    pwar_router_send_buffer(&linux_router, win_output_ptrs, WIN_BUFFER, CHANNELS, packets, 32, &packets_to_send);
                }
            }

            // NETWORK DELAY
            for (uint32_t p = 0; p < packets_to_send; ++p) {
                // Linux side (receiving):
                float linux_output_buffers[CHANNELS][WIN_BUFFER] = {0};
                float *linux_output_ptrs[CHANNELS];
                for (uint32_t ch = 0; ch < CHANNELS; ++ch) {
                    linux_output_ptrs[ch] = linux_output_buffers[ch];
                }

                int r = pwar_router_process_packet(&linux_router, &packets[p], linux_output_ptrs, WIN_BUFFER, CHANNELS);
                if (r == 1) {
                    // We now have a new package on the Linux side, push it to the receive buffer
                    pwar_rcv_buffer_add_buffer(linux_output_ptrs, WIN_BUFFER, CHANNELS);
                }
            }
        }

        // Linux side: Get a chunk from the receive buffer
        float linux_rcv_buffers[CHANNELS][CHUNK_SIZE] = {0};
        float *linux_rcv_ptrs[CHANNELS];
        for (uint32_t ch = 0; ch < CHANNELS; ++ch) {
            linux_rcv_ptrs[ch] = linux_rcv_buffers[ch];
        }

        pwar_rcv_get_chunk(linux_rcv_ptrs, CHANNELS);

        // Push the received chunk to the result samples
        for (uint32_t ch = 0; ch < CHANNELS; ++ch) {
            for (uint32_t s = 0; s < CHUNK_SIZE; ++s) {
                result_samples[ch][start + s] = linux_rcv_buffers[ch][s];
            }
        }
    }

    // The system delay is WIN_BUFFER (1024), but since we write each received chunk at 'start',
    // the first real output chunk (containing delayed input) is written at start = WIN_BUFFER - CHUNK_SIZE (896).
    // This is because the receive buffer outputs zeros until the delay is met, and then the first chunk of real data
    // is written at the index corresponding to the chunk that caused the buffer to fill up.
    // So, to compare input and output, we must use result_samples[0][s + WIN_BUFFER - CHUNK_SIZE] vs test_samples[0][s].
    // This is a common artifact in block-based streaming systems.

    uint32_t max_testable = n_test_samples - (WIN_BUFFER - CHUNK_SIZE);
    for (uint32_t s = 0; s < max_testable; ++s) {
        ck_assert_float_eq_tol(
            result_samples[0][s + WIN_BUFFER - CHUNK_SIZE],
            test_samples[0][s],
            0.0001f
        );
    }
}
END_TEST


Suite *send_receive_chain_suite(void) {
    Suite *s = suite_create("pwar_send_receive_chain");
    TCase *tc_core = tcase_create("Core");
    tcase_add_test(tc_core, test_send_and_rcv);
    suite_add_tcase(s, tc_core);
    return s;
}

int main(void) {
    int number_failed;
    Suite *s = send_receive_chain_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
