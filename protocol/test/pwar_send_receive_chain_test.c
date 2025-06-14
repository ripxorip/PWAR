#include <check.h>
#include <string.h>
#include <stdint.h>

#include "../pwar_send_buffer.h"
#include "../pwar_router.h"
#include "../pwar_rcv_buffer.h"

#define CHANNELS 2
#define CHUNK_SIZE 128
#define WIN_BUFFER 1024

static void fill_samples(float **samples, uint32_t channels, uint32_t n_samples, float value) {
    for (uint32_t ch = 0; ch < channels; ++ch) {
        for (uint32_t s = 0; s < n_samples; ++s) {
            samples[ch][s] = value + ch * 1000 + s;
        }
    }
}

START_TEST(test_send_and_rcv)
{
    //pwar_router_t linux_router;
    //pwar_router_t win_router;
    //pwar_router_init(&linux_router, CHANNELS);
    //pwar_router_init(&win_router, CHANNELS);
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
