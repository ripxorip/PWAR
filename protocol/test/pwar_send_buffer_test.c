#include <check.h>
#include "../pwar_send_buffer.h"
#include <string.h>

static void fill_samples(float *samples, uint32_t channels, uint32_t n_samples, uint32_t stride, float value) {
    for (uint32_t ch = 0; ch < channels; ++ch) {
        for (uint32_t s = 0; s < n_samples; ++s) {
            samples[ch * stride + s] = value + ch * 1000 + s;
        }
    }
}

START_TEST(test_send_buffer_empty)
{
    pwar_send_buffer_init(2, 256);
    ck_assert_int_eq(pwar_send_buffer_ready(), 0);
}
END_TEST

START_TEST(test_send_buffer_256)
{
    pwar_send_buffer_init(2, 256);
    float buf[2 * 128];
    fill_samples(buf, 2, 128, 128, 1.0f);
    ck_assert_int_eq(pwar_send_buffer_push(buf, 128, 2, 128), 128);
    ck_assert_int_eq(pwar_send_buffer_ready(), 0);
    fill_samples(buf, 2, 128, 128, 2.0f);
    ck_assert_int_eq(pwar_send_buffer_push(buf, 128, 2, 128), 256);
    ck_assert_int_eq(pwar_send_buffer_ready(), 1);
    float out_channels[2 * 256];
    uint32_t n_samples = 0;
    pwar_send_buffer_get(out_channels, &n_samples, 256);
    ck_assert_int_eq(n_samples, 256);
    ck_assert_float_eq_tol(out_channels[0 * 256 + 0], 1.0f, 0.0001f);
    ck_assert_float_eq_tol(out_channels[0 * 256 + 128], 2.0f, 0.0001f);
    ck_assert_float_eq_tol(out_channels[1 * 256 + 0], 1001.0f, 0.0001f);
    ck_assert_float_eq_tol(out_channels[1 * 256 + 128], 1002.0f, 0.0001f);
}
END_TEST

START_TEST(test_send_buffer_512)
{
    pwar_send_buffer_init(2, 512);
    float buf[2 * 128];
    for (int i = 0; i < 4; ++i) {
        fill_samples(buf, 2, 128, 128, (float)i);
        pwar_send_buffer_push(buf, 128, 2, 128);
    }
    ck_assert_int_eq(pwar_send_buffer_ready(), 1);
    float out_channels[2 * 512];
    uint32_t n_samples = 0;
    pwar_send_buffer_get(out_channels, &n_samples, 512);
    ck_assert_int_eq(n_samples, 512);
    ck_assert_float_eq_tol(out_channels[0 * 512 + 0], 0.0f, 0.0001f);
    ck_assert_float_eq_tol(out_channels[0 * 512 + 128], 1.0f, 0.0001f);
    ck_assert_float_eq_tol(out_channels[0 * 512 + 256], 2.0f, 0.0001f);
    ck_assert_float_eq_tol(out_channels[0 * 512 + 384], 3.0f, 0.0001f);
    ck_assert_float_eq_tol(out_channels[1 * 512 + 0], 1000.0f, 0.0001f);
    ck_assert_float_eq_tol(out_channels[1 * 512 + 128], 1001.0f, 0.0001f);
    ck_assert_float_eq_tol(out_channels[1 * 512 + 256], 1002.0f, 0.0001f);
    ck_assert_float_eq_tol(out_channels[1 * 512 + 384], 1003.0f, 0.0001f);
}
END_TEST

START_TEST(test_send_buffer_1024)
{
    pwar_send_buffer_init(2, 1024);
    float buf[2 * 128];
    for (int i = 0; i < 8; ++i) {
        fill_samples(buf, 2, 128, 128, (float)i);
        pwar_send_buffer_push(buf, 128, 2, 128);
    }
    ck_assert_int_eq(pwar_send_buffer_ready(), 1);
    float out_channels[2 * 1024];
    uint32_t n_samples = 0;
    pwar_send_buffer_get(out_channels, &n_samples, 1024);
    ck_assert_int_eq(n_samples, 1024);
    ck_assert_float_eq_tol(out_channels[0 * 1024 + 0], 0.0f, 0.0001f);
    ck_assert_float_eq_tol(out_channels[0 * 1024 + 128], 1.0f, 0.0001f);
    ck_assert_float_eq_tol(out_channels[0 * 1024 + 896], 7.0f, 0.0001f);
    ck_assert_float_eq_tol(out_channels[1 * 1024 + 0], 1000.0f, 0.0001f);
    ck_assert_float_eq_tol(out_channels[1 * 1024 + 128], 1001.0f, 0.0001f);
    ck_assert_float_eq_tol(out_channels[1 * 1024 + 896], 1007.0f, 0.0001f);
}
END_TEST

START_TEST(test_send_buffer_multiple_rounds)
{
    pwar_send_buffer_init(2, 256);
    float buf[2 * 128];
    // First round
    fill_samples(buf, 2, 128, 128, 10.0f);
    pwar_send_buffer_push(buf, 128, 2, 128);
    fill_samples(buf, 2, 128, 128, 20.0f);
    pwar_send_buffer_push(buf, 128, 2, 128);
    ck_assert_int_eq(pwar_send_buffer_ready(), 1);
    float out_channels[2 * 256];
    uint32_t n_samples = 0;
    pwar_send_buffer_get(out_channels, &n_samples, 256);
    ck_assert_int_eq(n_samples, 256);
    ck_assert_float_eq_tol(out_channels[0 * 256 + 0], 10.0f, 0.0001f);
    ck_assert_float_eq_tol(out_channels[0 * 256 + 128], 20.0f, 0.0001f);
    ck_assert_float_eq_tol(out_channels[1 * 256 + 0], 1010.0f, 0.0001f);
    ck_assert_float_eq_tol(out_channels[1 * 256 + 128], 1020.0f, 0.0001f);
    // Second round
    fill_samples(buf, 2, 128, 128, 30.0f);
    pwar_send_buffer_push(buf, 128, 2, 128);
    fill_samples(buf, 2, 128, 128, 40.0f);
    pwar_send_buffer_push(buf, 128, 2, 128);
    ck_assert_int_eq(pwar_send_buffer_ready(), 1);
    pwar_send_buffer_get(out_channels, &n_samples, 256);
    ck_assert_int_eq(n_samples, 256);
    ck_assert_float_eq_tol(out_channels[0 * 256 + 0], 30.0f, 0.0001f);
    ck_assert_float_eq_tol(out_channels[0 * 256 + 128], 40.0f, 0.0001f);
    ck_assert_float_eq_tol(out_channels[1 * 256 + 0], 1030.0f, 0.0001f);
    ck_assert_float_eq_tol(out_channels[1 * 256 + 128], 1040.0f, 0.0001f);
}
END_TEST

// Test pushing 2048 increasing samples into a 512-sized buffer, checking for correct delay
START_TEST(test_send_buffer_delay_512_on_2048_samples)
{
    // Initialize send buffer for 1 channel, buffer size 512
    pwar_send_buffer_init(1, 512);
    
    // Prepare input data: 2048 samples, increasing from 0 to 2047
    float input[2048];
    for (int i = 0; i < 2048; ++i) {
        input[i] = (float)i;
    }
    
    // Array to hold output results (delayed samples)
    float result[2048] = {0};
    int result_pos = 0;
    
    // Temporary buffer for pushing 128 samples at a time
    float buf[128];
    // Push 128 samples at a time, collect output when buffer is ready
    for (int i = 0; i < 2048; i += 128) {
        // Copy next 128 samples into chunk
        for (int j = 0; j < 128; ++j) {
            buf[j] = input[i + j];
        }
        pwar_send_buffer_push(buf, 128, 1, 128);
        // When buffer is ready, retrieve output and store in result
        if (pwar_send_buffer_ready()) {
            float out_channels[512];
            uint32_t n_samples = 0;
            pwar_send_buffer_get(out_channels, &n_samples, 512);
            // Copy output to result array
            for (uint32_t k = 0; k < n_samples; ++k) {
                result[result_pos++] = out_channels[k];
            }
        }
    }
    // At the end, result_pos should be 2048
    ck_assert_int_eq(result_pos, 2048);
    // The result should match the input, but delayed by 512 samples
    // That is, result[i] == input[i] for all i
    for (int i = 0; i < 2048; ++i) {
        ck_assert_float_eq_tol(result[i], input[i], 0.0001f);
    }
}
END_TEST

// Test that the buffer does not become ready until the full buffer size is met, and only after the correct number of chunks
START_TEST(test_send_buffer_ready_only_on_full)
{
    // Use buffer size 512, chunk size 128
    const int buffer_size = 512;
    const int chunk_size = 128;
    const int n_chunks = buffer_size / chunk_size;
    pwar_send_buffer_init(1, buffer_size);
    float buf[128];
    // Fill with dummy data
    for (int j = 0; j < chunk_size; ++j) buf[j] = (float)j;

    // 1. Buffer should not be ready until buffer_size is met
    for (int i = 0; i < n_chunks - 1; ++i) {
        pwar_send_buffer_push(buf, chunk_size, 1, chunk_size);
        ck_assert_int_eq(pwar_send_buffer_ready(), 0);
    }
    // Last chunk should make it ready
    pwar_send_buffer_push(buf, chunk_size, 1, chunk_size);
    ck_assert_int_eq(pwar_send_buffer_ready(), 1);
    // Read out the buffer to trigger reset
    float out_channels[512];
    uint32_t n_samples = 0;
    pwar_send_buffer_get(out_channels, &n_samples, 512);
    ck_assert_int_eq(n_samples, buffer_size);

    // Repeat: should not be ready until full again
    for (int i = 0; i < n_chunks - 1; ++i) {
        pwar_send_buffer_push(buf, chunk_size, 1, chunk_size);
        ck_assert_int_eq(pwar_send_buffer_ready(), 0);
    }
    pwar_send_buffer_push(buf, chunk_size, 1, chunk_size);
    ck_assert_int_eq(pwar_send_buffer_ready(), 1);
    // Read out again to reset
    pwar_send_buffer_get(out_channels, &n_samples, 512);
    ck_assert_int_eq(n_samples, buffer_size);
}
END_TEST

Suite *send_buffer_suite(void) {
    Suite *s = suite_create("pwar_send_buffer");
    TCase *tc_core = tcase_create("Core");
    tcase_add_test(tc_core, test_send_buffer_empty);
    tcase_add_test(tc_core, test_send_buffer_256);
    tcase_add_test(tc_core, test_send_buffer_512);
    tcase_add_test(tc_core, test_send_buffer_1024);
    tcase_add_test(tc_core, test_send_buffer_multiple_rounds);
    tcase_add_test(tc_core, test_send_buffer_delay_512_on_2048_samples);
    tcase_add_test(tc_core, test_send_buffer_ready_only_on_full);
    suite_add_tcase(s, tc_core);
    return s;
}

int main(void) {
    int number_failed;
    Suite *s = send_buffer_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
