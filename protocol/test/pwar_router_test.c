#include <check.h>
#include <stdio.h>
#include "../pwar_router.h"

static void fill_samples(float *samples, uint32_t channels, uint32_t n_samples, uint32_t stride, float value) {
    for (uint32_t ch = 0; ch < channels; ++ch) {
        for (uint32_t s = 0; s < n_samples; ++s) {
            samples[ch * stride + s] = value + ch * 1000 + s;
        }
    }
}

START_TEST(test_router_send_and_process)
{
    pwar_router_t router;
    const uint32_t channels = 2;
    const uint32_t n_samples = 256;
    const uint32_t stride = n_samples;
    pwar_router_init(&router, channels);
    float samples[channels * n_samples];
    fill_samples(samples, channels, n_samples, stride, 1.0f);
    pwar_packet_t packets[16];
    uint32_t packets_to_send = 0;
    int ret = pwar_router_send_buffer(&router, samples, n_samples, channels, stride, packets, 16, &packets_to_send);
    ck_assert_int_eq(ret, 1);
    ck_assert_int_gt(packets_to_send, 0);
    float output[channels * n_samples];
    for (uint32_t i = 0; i < packets_to_send; ++i) {
        int ready = pwar_router_process_packet(&router, &packets[i], output, n_samples, channels, stride);
        if (i < packets_to_send - 1)
            ck_assert_int_eq(ready, 0);
        else
            ck_assert_int_eq(ready, 1);
    }
    for (uint32_t ch = 0; ch < channels; ++ch) {
        for (uint32_t s = 0; s < n_samples; ++s) {
            ck_assert_float_eq_tol(output[ch * stride + s], samples[ch * stride + s], 0.0001f);
        }
    }
}
END_TEST

START_TEST(test_router_out_of_order)
{
    pwar_router_t router;
    const uint32_t channels = 2;
    const uint32_t n_samples = 1024;
    const uint32_t stride = n_samples;
    pwar_router_init(&router, channels);
    float samples[channels * n_samples];
    fill_samples(samples, channels, n_samples, stride, 2.0f);
    pwar_packet_t packets[16];
    uint32_t packets_to_send = 0;
    int ret = pwar_router_send_buffer(&router, samples, n_samples, channels, stride, packets, 16, &packets_to_send);
    ck_assert_int_eq(ret, 1);
    ck_assert_int_gt(packets_to_send, 0);
    float output[channels * n_samples];
    // Deliver packets out of order
    uint32_t mid = packets_to_send / 2;
    int ready = pwar_router_process_packet(&router, &packets[mid], output, n_samples, channels, stride);
    ck_assert_int_eq(ready, 0);
    for (uint32_t i = 0; i < packets_to_send; ++i) {
        if (i == mid) continue;
        ready = pwar_router_process_packet(&router, &packets[i], output, n_samples, channels, stride);
    }
    ck_assert_int_eq(ready, 1);
    for (uint32_t ch = 0; ch < channels; ++ch) {
        for (uint32_t s = 0; s < n_samples; ++s) {
            ck_assert_float_eq_tol(output[ch * stride + s], samples[ch * stride + s], 0.0001f);
        }
    }
}
END_TEST

START_TEST(test_router_multiple_seq)
{
    pwar_router_t router;
    const uint32_t channels = 2;
    const uint32_t n_samples = 1024;
    const uint32_t stride = n_samples;
    pwar_router_init(&router, channels);
    float samples[channels * n_samples];
    float output[channels * n_samples];
    pwar_packet_t packets[16];
    uint32_t packets_to_send = 0;
    uint64_t seq = 100;
    for (int round = 0; round < 3; ++round, ++seq) {
        fill_samples(samples, channels, n_samples, stride, 10.0f * (round + 1));
        int ret = pwar_router_send_buffer(&router, samples, n_samples, channels, stride, packets, 16, &packets_to_send);
        ck_assert_int_eq(ret, 1);
        ck_assert_int_gt(packets_to_send, 0);
        // Set the seq for all packets in this round
        for (uint32_t i = 0; i < packets_to_send; ++i) {
            packets[i].seq = seq;
        }
        int ready = 0;
        for (uint32_t i = 0; i < packets_to_send; ++i) {
            ready = pwar_router_process_packet(&router, &packets[i], output, n_samples, channels, stride);
        }
        ck_assert_int_eq(ready, 1);
        for (uint32_t ch = 0; ch < channels; ++ch) {
            for (uint32_t s = 0; s < n_samples; ++s) {
                ck_assert_float_eq_tol(output[ch * stride + s], samples[ch * stride + s], 0.0001f);
            }
        }
    }
}
END_TEST

Suite *pwar_router_suite(void) {
    Suite *s;
    TCase *tc_core;
    s = suite_create("pwar_router");
    tc_core = tcase_create("Core");
    tcase_add_test(tc_core, test_router_send_and_process);
    tcase_add_test(tc_core, test_router_out_of_order);
    tcase_add_test(tc_core, test_router_multiple_seq);
    suite_add_tcase(s, tc_core);
    return s;
}

int main(void) {
    int number_failed;
    Suite *s;
    SRunner *sr;
    s = pwar_router_suite();
    sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}