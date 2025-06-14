#include <check.h>
#include "../pwar_send_buffer.h"
#include <string.h>

static void fill_samples(float **samples, uint32_t channels, uint32_t n_samples, float value) {
    for (uint32_t ch = 0; ch < channels; ++ch) {
        for (uint32_t s = 0; s < n_samples; ++s) {
            samples[ch][s] = value + ch * 1000 + s;
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
    float *buf[2];
    float ch1[128], ch2[128];
    buf[0] = ch1; buf[1] = ch2;
    fill_samples(buf, 2, 128, 1.0f);
    ck_assert_int_eq(pwar_send_buffer_push(buf, 128), 128);
    ck_assert_int_eq(pwar_send_buffer_ready(), 0);
    fill_samples(buf, 2, 128, 2.0f);
    ck_assert_int_eq(pwar_send_buffer_push(buf, 128), 256);
    ck_assert_int_eq(pwar_send_buffer_ready(), 1);
    float *out_channels[2];
    uint32_t n_samples = 0;
    pwar_send_buffer_get(out_channels, &n_samples);
    ck_assert_int_eq(n_samples, 256);
    ck_assert_float_eq_tol(out_channels[0][0], 1.0f, 0.0001f);
    ck_assert_float_eq_tol(out_channels[0][128], 2.0f, 0.0001f);
    ck_assert_float_eq_tol(out_channels[1][0], 1001.0f, 0.0001f);
    ck_assert_float_eq_tol(out_channels[1][128], 1002.0f, 0.0001f);
    pwar_send_buffer_reset();
    ck_assert_int_eq(pwar_send_buffer_ready(), 0);
}
END_TEST

START_TEST(test_send_buffer_512)
{
    pwar_send_buffer_init(2, 512);
    float *buf[2];
    float ch1[128], ch2[128];
    buf[0] = ch1; buf[1] = ch2;
    for (int i = 0; i < 4; ++i) {
        fill_samples(buf, 2, 128, (float)i);
        pwar_send_buffer_push(buf, 128);
    }
    ck_assert_int_eq(pwar_send_buffer_ready(), 1);
    float *out_channels[2];
    uint32_t n_samples = 0;
    pwar_send_buffer_get(out_channels, &n_samples);
    ck_assert_int_eq(n_samples, 512);
    ck_assert_float_eq_tol(out_channels[0][0], 0.0f, 0.0001f);
    ck_assert_float_eq_tol(out_channels[0][128], 1.0f, 0.0001f);
    ck_assert_float_eq_tol(out_channels[0][256], 2.0f, 0.0001f);
    ck_assert_float_eq_tol(out_channels[0][384], 3.0f, 0.0001f);
    ck_assert_float_eq_tol(out_channels[1][0], 1000.0f, 0.0001f);
    ck_assert_float_eq_tol(out_channels[1][128], 1001.0f, 0.0001f);
    ck_assert_float_eq_tol(out_channels[1][256], 1002.0f, 0.0001f);
    ck_assert_float_eq_tol(out_channels[1][384], 1003.0f, 0.0001f);
}
END_TEST

START_TEST(test_send_buffer_1024)
{
    pwar_send_buffer_init(2, 1024);
    float *buf[2];
    float ch1[128], ch2[128];
    buf[0] = ch1; buf[1] = ch2;
    for (int i = 0; i < 8; ++i) {
        fill_samples(buf, 2, 128, (float)i);
        pwar_send_buffer_push(buf, 128);
    }
    ck_assert_int_eq(pwar_send_buffer_ready(), 1);
    float *out_channels[2];
    uint32_t n_samples = 0;
    pwar_send_buffer_get(out_channels, &n_samples);
    ck_assert_int_eq(n_samples, 1024);
    ck_assert_float_eq_tol(out_channels[0][0], 0.0f, 0.0001f);
    ck_assert_float_eq_tol(out_channels[0][128], 1.0f, 0.0001f);
    ck_assert_float_eq_tol(out_channels[0][896], 7.0f, 0.0001f);
    ck_assert_float_eq_tol(out_channels[1][0], 1000.0f, 0.0001f);
    ck_assert_float_eq_tol(out_channels[1][128], 1001.0f, 0.0001f);
    ck_assert_float_eq_tol(out_channels[1][896], 1007.0f, 0.0001f);
}
END_TEST

START_TEST(test_send_buffer_multiple_rounds)
{
    pwar_send_buffer_init(2, 256);
    float *buf[2];
    float ch1[128], ch2[128];
    buf[0] = ch1; buf[1] = ch2;
    // First round
    fill_samples(buf, 2, 128, 10.0f);
    pwar_send_buffer_push(buf, 128);
    fill_samples(buf, 2, 128, 20.0f);
    pwar_send_buffer_push(buf, 128);
    ck_assert_int_eq(pwar_send_buffer_ready(), 1);
    float *out_channels[2];
    uint32_t n_samples = 0;
    pwar_send_buffer_get(out_channels, &n_samples);
    ck_assert_int_eq(n_samples, 256);
    ck_assert_float_eq_tol(out_channels[0][0], 10.0f, 0.0001f);
    ck_assert_float_eq_tol(out_channels[0][128], 20.0f, 0.0001f);
    ck_assert_float_eq_tol(out_channels[1][0], 1010.0f, 0.0001f);
    ck_assert_float_eq_tol(out_channels[1][128], 1020.0f, 0.0001f);
    pwar_send_buffer_reset();
    // Second round
    fill_samples(buf, 2, 128, 30.0f);
    pwar_send_buffer_push(buf, 128);
    fill_samples(buf, 2, 128, 40.0f);
    pwar_send_buffer_push(buf, 128);
    ck_assert_int_eq(pwar_send_buffer_ready(), 1);
    pwar_send_buffer_get(out_channels, &n_samples);
    ck_assert_int_eq(n_samples, 256);
    ck_assert_float_eq_tol(out_channels[0][0], 30.0f, 0.0001f);
    ck_assert_float_eq_tol(out_channels[0][128], 40.0f, 0.0001f);
    ck_assert_float_eq_tol(out_channels[1][0], 1030.0f, 0.0001f);
    ck_assert_float_eq_tol(out_channels[1][128], 1040.0f, 0.0001f);
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
