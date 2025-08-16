/*
 * Integration Test: Send-Receive Chain for PWAR Protocol
 *
 * This test simulates the full data flow of the PWAR protocol between two endpoints (e.g., Linux and Windows).
 * It demonstrates how to use the main PWAR components together:
 *
 *   - pwar_send_buffer: Buffers outgoing audio data in blocks/chunks.
 *   - pwar_router: Serializes buffered data into packets for network transfer, and deserializes received packets.
 *   - pwar_rcv_buffer: Buffers received audio data for consumption.
 *
 * Data Flow (as simulated in this test):
 *
 *   1. Application pushes audio data to pwar_send_buffer (Linux side).
 *   2. When enough data is buffered, pwar_send_buffer provides a block to pwar_router.
 *   3. pwar_router serializes the block into packets (to be sent over the network).
 *   4. Each packet is assigned a sequence number (seq) that uniquely identifies the buffer transaction.
 *      All packets for a given buffer share the same seq value. The seq is incremented for each new buffer.
 *   5. Packets are (in reality) sent over the network to the other endpoint (Windows side).
 *   6. On the receiving side, pwar_router uses the seq field to correctly reassemble packets into the original buffer,
 *      regardless of arrival order or network reordering.
 *   7. Reconstructed blocks are sent back (simulating a round-trip or echo for test purposes), again with a new seq value.
 *   8. The original sender receives the returned packets, processes them with pwar_router, and adds the result to pwar_rcv_buffer.
 *   9. Application reads output audio data from pwar_rcv_buffer.
 *
 * In production:
 *   - Steps 1, 2, and 9 are typically in the audio callback or main processing loop.
 *   - Steps 3, 4, and 6 are called when sending/receiving network packets.
 *   - Step 8 is called when a full block is reconstructed from received packets.
 *   - The seq field is critical for robust UDP streaming, ensuring correct buffer assembly even with out-of-order delivery.
 *
 * This test can be used as a reference for integrating the send/receive chain in your own application.
 *
 * To run the test:
 *   cd protocol/test && make && ./_out/pwar_send_receive_chain_test
 */

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
static void fill_test_sine_wave(float *test_samples, uint32_t channels, uint32_t n_test_samples, float frequency, float sample_rate) {
    for (uint32_t ch = 0; ch < channels; ++ch) {
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
    float test_samples[CHANNELS * n_test_samples];
    fill_test_sine_wave(test_samples, CHANNELS, n_test_samples, 440.0f, 48000.0f);

    float result_samples[CHANNELS * n_test_samples];

    // Initialize Linux side
    pwar_router_init(&linux_router, CHANNELS);
    pwar_send_buffer_init(CHANNELS, WIN_BUFFER);
    // The rcv buffer doesnt need any initialization, it is a singleton API

    // Initialize Windows side
    pwar_router_init(&win_router, CHANNELS);

    uint32_t seq = 0;

    // Loop over the test_samples in chunks
    for (uint32_t start = 0; start < n_test_samples; start += CHUNK_SIZE) {
        // Linux side:
        float chunk_buf[CHANNELS * CHUNK_SIZE];
        for (uint32_t ch = 0; ch < CHANNELS; ++ch) {
            for (uint32_t s = 0; s < CHUNK_SIZE; ++s) {
                chunk_buf[ch * CHUNK_SIZE + s] = test_samples[ch * n_test_samples + start + s];
            }
        }
        pwar_send_buffer_push(chunk_buf, CHUNK_SIZE, CHANNELS, CHUNK_SIZE);

        if (pwar_send_buffer_ready()) {
            float linux_send_samples[CHANNELS * WIN_BUFFER];
            uint32_t n_samples = 0;
            pwar_send_buffer_get(linux_send_samples, &n_samples, WIN_BUFFER);

            pwar_packet_t packets[32] = {0};
            uint32_t packets_to_send = 0;
            pwar_router_send_buffer(&linux_router, linux_send_samples, n_samples, CHANNELS, WIN_BUFFER, packets, 32, &packets_to_send);
            seq++;
            // Set seq for all packets in this buffer
            for (uint32_t i = 0; i < packets_to_send; ++i) {
                packets[i].seq = seq;
            }

            // NETWORK DELAY

            // (Fake sending the packets to Windows side)
            // Windows side (receiving):
            for (uint32_t p = 0; p < packets_to_send; ++p) {
                float win_output_buffers[CHANNELS * WIN_BUFFER] = {0};
                int r = pwar_router_process_packet(&win_router, &packets[p], win_output_buffers, WIN_BUFFER, CHANNELS, WIN_BUFFER);

                if (r == 1) {
                    // FAKE PROCESSING..
                    // Packet ready and we have output to send to Linux side
                    pwar_router_send_buffer(&win_router, win_output_buffers, WIN_BUFFER, CHANNELS, WIN_BUFFER, packets, 32, &packets_to_send);
                    // Set seq for all packets in this buffer
                    for (uint32_t i = 0; i < packets_to_send; ++i) {
                        packets[i].seq = seq;
                    }
                }
            }

            // NETWORK DELAY
            for (uint32_t p = 0; p < packets_to_send; ++p) {
                float linux_output_buffers[CHANNELS * WIN_BUFFER] = {0};
                int r = pwar_router_process_packet(&linux_router, &packets[p], linux_output_buffers, WIN_BUFFER, CHANNELS, WIN_BUFFER);
                if (r == 1) {
                    pwar_rcv_buffer_add_buffer(linux_output_buffers, WIN_BUFFER, CHANNELS, WIN_BUFFER);
                }
            }
        }

        // Linux side: Get a chunk from the receive buffer
        float linux_rcv_buffers[CHANNELS * CHUNK_SIZE] = {0};
        pwar_rcv_get_chunk(linux_rcv_buffers, CHANNELS, CHUNK_SIZE);

        // Push the received chunk to the result samples
        for (uint32_t ch = 0; ch < CHANNELS; ++ch) {
            for (uint32_t s = 0; s < CHUNK_SIZE; ++s) {
                result_samples[ch * n_test_samples + start + s] = linux_rcv_buffers[ch * CHUNK_SIZE + s];
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
            result_samples[0 * n_test_samples + s + WIN_BUFFER - CHUNK_SIZE],
            test_samples[0 * n_test_samples + s],
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
