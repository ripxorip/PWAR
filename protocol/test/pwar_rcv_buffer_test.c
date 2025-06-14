#include <check.h>
#include <string.h>
#include <stdio.h>
#include "../pwar_rcv_buffer.h"

#define TEST_CHANNELS 2
#define TEST_CHUNK_SIZE 128
#define TEST_BUF_SIZE 512

static void fill_samples(float **samples, uint32_t channels, uint32_t n_samples, float value) {
    for (uint32_t ch = 0; ch < channels; ++ch) {
        for (uint32_t s = 0; s < n_samples; ++s) {
            samples[ch][s] = value + ch * 1000 + s;
        }
    }
}

START_TEST(test_rcv_buffer_empty)
{
    ck_assert_int_eq(1, 1); // Placeholder test
}
END_TEST

START_TEST(test_rcv_buffer_silence_before_fill)
{
    float *chunks[TEST_CHANNELS];
    float ch1[TEST_CHUNK_SIZE], ch2[TEST_CHUNK_SIZE];
    chunks[0] = ch1; chunks[1] = ch2;
    int ret = pwar_rcv_get_chunk(chunks, TEST_CHANNELS);
    ck_assert_int_eq(ret, 0);
    for (int ch = 0; ch < TEST_CHANNELS; ++ch)
        for (int s = 0; s < TEST_CHUNK_SIZE; ++s)
            ck_assert_float_eq_tol(chunks[ch][s], 0.0f, 0.0001f);
}
END_TEST

START_TEST(test_rcv_buffer_fill_and_read)
{
    float *buf[TEST_CHANNELS];
    float b1[TEST_BUF_SIZE], b2[TEST_BUF_SIZE];
    buf[0] = b1; buf[1] = b2;
    fill_samples(buf, TEST_CHANNELS, TEST_BUF_SIZE, 1.0f);
    pwar_rcv_buffer_add_buffer(buf, TEST_BUF_SIZE, TEST_CHANNELS);
    float *chunks[TEST_CHANNELS];
    float ch1[TEST_CHUNK_SIZE], ch2[TEST_CHUNK_SIZE];
    chunks[0] = ch1; chunks[1] = ch2;
    for (int i = 0; i < 4; ++i) {
        int ret = pwar_rcv_get_chunk(chunks, TEST_CHANNELS);
        ck_assert_int_eq(ret, 1);
        for (int ch = 0; ch < TEST_CHANNELS; ++ch)
            for (int s = 0; s < TEST_CHUNK_SIZE; ++s)
                ck_assert_float_eq_tol(chunks[ch][s], 1.0f + ch * 1000 + i * TEST_CHUNK_SIZE + s, 0.0001f);
    }
    // Now buffer is empty, should get silence
    int ret = pwar_rcv_get_chunk(chunks, TEST_CHANNELS);
    ck_assert_int_eq(ret, 0);
    for (int ch = 0; ch < TEST_CHANNELS; ++ch)
        for (int s = 0; s < TEST_CHUNK_SIZE; ++s)
            ck_assert_float_eq_tol(chunks[ch][s], 0.0f, 0.0001f);
}
END_TEST

Suite *rcv_buffer_suite(void) {
    Suite *s = suite_create("pwar_rcv_buffer");
    TCase *tc_core = tcase_create("Core");
    tcase_add_test(tc_core, test_rcv_buffer_empty);
    tcase_add_test(tc_core, test_rcv_buffer_silence_before_fill);
    tcase_add_test(tc_core, test_rcv_buffer_fill_and_read);
    suite_add_tcase(s, tc_core);
    return s;
}

int main(void) {
    int number_failed;
    Suite *s = rcv_buffer_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
