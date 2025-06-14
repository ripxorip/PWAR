#include <check.h>
#include <stdio.h>
#include "../pwar_router.h"

START_TEST(test_send_buffer)
{
    const uint32_t channels = 2;
    const uint32_t test_sizes[] = {128, 256, 512, 1024, 2048, 4096};
    for (size_t t = 0; t < sizeof(test_sizes)/sizeof(test_sizes[0]); ++t) {
        uint32_t n_samples = test_sizes[t];
        pwar_router_t router;
        pwar_router_init(&router, channels); // Only channel count needed
        float ch0[4096], ch1[4096];
        for (uint32_t i = 0; i < n_samples; ++i) {
            ch0[i] = (float)i;
            ch1[i] = (float)(i + 1000);
        }
        float *samples[2] = { ch0, ch1 };
        uint32_t max_packets = (n_samples + PWAR_PACKET_CHUNK_SIZE - 1) / PWAR_PACKET_CHUNK_SIZE;
        pwar_packet_t packets[32] = {0};
        uint32_t packets_to_send = 0;
        int ret = pwar_router_send_buffer(&router, samples, n_samples, channels, packets, max_packets, &packets_to_send);
        ck_assert_msg(ret == 0, "n_samples=%u ret=%d", n_samples, ret);
        for (uint32_t p = 0; p < packets_to_send; ++p) {
            ck_assert_msg(packets[p].num_packets == packets_to_send, "packet %u num_packets=%u (expected %u)", p, packets[p].num_packets, packets_to_send);
            ck_assert_msg(packets[p].packet_index == p, "packet %u packet_index=%u (expected %u)", p, packets[p].packet_index, p);
            uint32_t expected_samples = ((p + 1) * PWAR_PACKET_CHUNK_SIZE <= n_samples) ? PWAR_PACKET_CHUNK_SIZE : (n_samples - p * PWAR_PACKET_CHUNK_SIZE);
            ck_assert_msg(packets[p].n_samples == expected_samples, "packet %u n_samples=%u (expected %u)", p, packets[p].n_samples, expected_samples);
        }
    }
}
END_TEST

START_TEST(test_process_packet)
{
    const uint32_t channels = 2;
    const uint32_t test_sizes[] = {128, 256, 512, 1024, 2048, 4096};
    for (size_t t = 0; t < sizeof(test_sizes)/sizeof(test_sizes[0]); ++t) {
        uint32_t n_samples = test_sizes[t];
        pwar_router_t router;
        pwar_router_init(&router, channels); // Only channel count needed
        float ch0[4096], ch1[4096];
        for (uint32_t i = 0; i < n_samples; ++i) {
            ch0[i] = (float)i;
            ch1[i] = (float)(i + 1000);
        }
        float *samples[2] = { ch0, ch1 };
        uint32_t max_packets = (n_samples + PWAR_PACKET_CHUNK_SIZE - 1) / PWAR_PACKET_CHUNK_SIZE;
        pwar_packet_t packets[32] = {0};
        uint32_t packets_to_send = 0;
        int ret = pwar_router_send_buffer(&router, samples, n_samples, channels, packets, max_packets, &packets_to_send);
        ck_assert_msg(ret == 0, "n_samples=%u ret=%d", n_samples, ret);
        float out_ch0[4096] = {0}, out_ch1[4096] = {0};
        float *output_buffers[2] = { out_ch0, out_ch1 };
        int output_ready = 0;
        for (uint32_t p = 0; p < packets_to_send; ++p) {
            int r = pwar_router_process_packet(&router, &packets[p], output_buffers, n_samples, channels);
            if (p < packets_to_send - 1) {
                ck_assert_msg(r == 0, "output ready too early at packet %u", p);
            }
            if (p == packets_to_send - 1) {
                ck_assert_msg(r == 1, "output not ready at last packet (n_samples=%u)", n_samples);
                output_ready = 1;
            }
        }
        if (output_ready) {
            for (uint32_t i = 0; i < n_samples; ++i) {
                ck_assert_msg(out_ch0[i] == ch0[i] && out_ch1[i] == ch1[i],
                    "output mismatch at sample %u: ch0=%.1f/%.1f ch1=%.1f/%.1f", i, out_ch0[i], ch0[i], out_ch1[i], ch1[i]);
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
    tcase_add_test(tc_core, test_send_buffer);
    tcase_add_test(tc_core, test_process_packet);
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