#include <check.h>
#include <string.h>
#include <stdio.h>
#include "../pwar_rcv_buffer.h"

#define TEST_CHANNELS 2
#define TEST_CHUNK_SIZE 128
#define TEST_BUF_SIZE 512

// Helper to fill a buffer with predictable sample values for testing
static void fill_samples(float **samples, uint32_t channels, uint32_t n_samples, float value) {
    for (uint32_t ch = 0; ch < channels; ++ch) {
        for (uint32_t s = 0; s < n_samples; ++s) {
            samples[ch][s] = value + ch * 1000 + s;
        }
    }
}

// Test: When the receive buffer is empty, pwar_rcv_get_chunk should return silence (all zeros)
START_TEST(test_rcv_buffer_silence_before_fill)
{
    float *chunks[TEST_CHANNELS];
    float ch1[TEST_CHUNK_SIZE], ch2[TEST_CHUNK_SIZE];
    chunks[0] = ch1; chunks[1] = ch2;
    // Try to get a chunk before any data is added
    int ret = pwar_rcv_get_chunk(chunks, TEST_CHANNELS);
    ck_assert_int_eq(ret, 0); // Should indicate silence
    // All samples should be zero
    for (int ch = 0; ch < TEST_CHANNELS; ++ch)
        for (int s = 0; s < TEST_CHUNK_SIZE; ++s)
            ck_assert_float_eq_tol(chunks[ch][s], 0.0f, 0.0001f);
}
END_TEST

// Test: Fill the buffer, read out all chunks, then verify silence after buffer is empty
START_TEST(test_rcv_buffer_fill_and_read)
{
    float *buf[TEST_CHANNELS];
    float b1[TEST_BUF_SIZE], b2[TEST_BUF_SIZE];
    buf[0] = b1; buf[1] = b2;
    // Fill the buffer with predictable values
    fill_samples(buf, TEST_CHANNELS, TEST_BUF_SIZE, 1.0f);
    pwar_rcv_buffer_add_buffer(buf, TEST_BUF_SIZE, TEST_CHANNELS);

    float *chunks[TEST_CHANNELS];
    float ch1[TEST_CHUNK_SIZE], ch2[TEST_CHUNK_SIZE];
    chunks[0] = ch1; chunks[1] = ch2;
    // Read out all available chunks (should be 4 chunks for 512 samples at 128 per chunk)
    for (int i = 0; i < 4; ++i) {
        int ret = pwar_rcv_get_chunk(chunks, TEST_CHANNELS);
        ck_assert_int_eq(ret, 1); // Should indicate data was read
        // Check that each sample matches what fill_samples wrote
        for (int ch = 0; ch < TEST_CHANNELS; ++ch)
            for (int s = 0; s < TEST_CHUNK_SIZE; ++s)
                ck_assert_float_eq_tol(chunks[ch][s], 1.0f + ch * 1000 + i * TEST_CHUNK_SIZE + s, 0.0001f);
    }
    // Now buffer is empty, should get silence again
    int ret = pwar_rcv_get_chunk(chunks, TEST_CHANNELS);
    ck_assert_int_eq(ret, 0);
    for (int ch = 0; ch < TEST_CHANNELS; ++ch)
        for (int s = 0; s < TEST_CHUNK_SIZE; ++s)
            ck_assert_float_eq_tol(chunks[ch][s], 0.0f, 0.0001f);
}
END_TEST

// Test suite setup
Suite *rcv_buffer_suite(void) {
    Suite *s = suite_create("pwar_rcv_buffer");
    TCase *tc_core = tcase_create("Core");
    tcase_add_test(tc_core, test_rcv_buffer_silence_before_fill);
    tcase_add_test(tc_core, test_rcv_buffer_fill_and_read);
    suite_add_tcase(s, tc_core);
    return s;
}

// Main entry for running the test suite
int main(void) {
    int number_failed;
    Suite *s = rcv_buffer_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
