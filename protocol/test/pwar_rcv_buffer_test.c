#include <check.h>
#include <string.h>
#include <stdio.h>
#include "../pwar_rcv_buffer.h"

#define TEST_CHANNELS 2
#define TEST_CHUNK_SIZE 128
#define TEST_BUF_SIZE 512

// Helper to fill a buffer with predictable sample values for testing
static void fill_samples(float *samples, uint32_t channels, uint32_t n_samples, uint32_t stride, float value) {
    for (uint32_t ch = 0; ch < channels; ++ch) {
        for (uint32_t s = 0; s < n_samples; ++s) {
            samples[ch * stride + s] = value + ch * 1000 + s;
        }
    }
}

// Test: When the receive buffer is empty, pwar_rcv_get_chunk should return silence (all zeros)
START_TEST(test_rcv_buffer_silence_before_fill)
{
    float chunks[TEST_CHANNELS * TEST_CHUNK_SIZE] = {0};
    int ret = pwar_rcv_get_chunk(chunks, TEST_CHANNELS, TEST_CHUNK_SIZE);
    ck_assert_int_eq(ret, 0); // Should indicate silence
    for (int ch = 0; ch < TEST_CHANNELS; ++ch)
        for (int s = 0; s < TEST_CHUNK_SIZE; ++s)
            ck_assert_float_eq_tol(chunks[ch * TEST_CHUNK_SIZE + s], 0.0f, 0.0001f);
}
END_TEST

// Test: Fill the buffer, read out all chunks, then verify silence after buffer is empty
START_TEST(test_rcv_buffer_fill_and_read)
{
    float buf[TEST_CHANNELS * TEST_BUF_SIZE];
    fill_samples(buf, TEST_CHANNELS, TEST_BUF_SIZE, TEST_BUF_SIZE, 1.0f);
    pwar_rcv_buffer_add_buffer(buf, TEST_BUF_SIZE, TEST_CHANNELS, TEST_BUF_SIZE);

    float chunks[TEST_CHANNELS * TEST_CHUNK_SIZE];
    for (int i = 0; i < 4; ++i) {
        int ret = pwar_rcv_get_chunk(chunks, TEST_CHANNELS, TEST_CHUNK_SIZE);
        ck_assert_int_eq(ret, 1); // Should indicate data was read
        for (int ch = 0; ch < TEST_CHANNELS; ++ch)
            for (int s = 0; s < TEST_CHUNK_SIZE; ++s)
                ck_assert_float_eq_tol(chunks[ch * TEST_CHUNK_SIZE + s], 1.0f + ch * 1000 + i * TEST_CHUNK_SIZE + s, 0.0001f);
    }
    int ret = pwar_rcv_get_chunk(chunks, TEST_CHANNELS, TEST_CHUNK_SIZE);
    ck_assert_int_eq(ret, 0);
    for (int ch = 0; ch < TEST_CHANNELS; ++ch)
        for (int s = 0; s < TEST_CHUNK_SIZE; ++s)
            ck_assert_float_eq_tol(chunks[ch * TEST_CHUNK_SIZE + s], 0.0f, 0.0001f);
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
