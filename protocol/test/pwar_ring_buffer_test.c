#include <check.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <math.h>
#include "../pwar_ring_buffer.h"

#define TEST_CHANNELS 2
#define TEST_DEPTH 1024
#define TEST_EXPECTED_BUFFER_SIZE 256

// Helper function to fill buffer with predictable test data
static void fill_test_data(float *buffer, uint32_t channels, uint32_t n_samples, float base_value) {
    for (uint32_t sample = 0; sample < n_samples; sample++) {
        for (uint32_t ch = 0; ch < channels; ch++) {
            buffer[sample * channels + ch] = base_value + sample + (ch * 1000.0f);
        }
    }
}

// Helper function to verify test data
static void verify_test_data(float *buffer, uint32_t channels, uint32_t n_samples, float base_value, const char *context) {
    for (uint32_t sample = 0; sample < n_samples; sample++) {
        for (uint32_t ch = 0; ch < channels; ch++) {
            float expected = base_value + sample + (ch * 1000.0f);
            float actual = buffer[sample * channels + ch];
            ck_assert_msg(fabsf(actual - expected) < 0.0001f, 
                         "%s: Sample mismatch at [%d][%d]: expected %f, got %f", 
                         context, sample, ch, expected, actual);
        }
    }
}

// Test basic initialization and cleanup
START_TEST(test_ring_buffer_init_free)
{
    pwar_ring_buffer_init(TEST_DEPTH, TEST_CHANNELS, TEST_EXPECTED_BUFFER_SIZE);
    
    // Check initial state
    ck_assert_uint_eq(pwar_ring_buffer_get_overruns(), 0);
    ck_assert_uint_eq(pwar_ring_buffer_get_underruns(), 0);
    
    uint32_t expected_prefill = TEST_DEPTH;
    ck_assert_uint_eq(pwar_ring_buffer_get_available(), expected_prefill);
    
    pwar_ring_buffer_free();
}
END_TEST

// Test basic push/pop functionality
START_TEST(test_ring_buffer_basic_push_pop)
{
    pwar_ring_buffer_init(TEST_DEPTH, TEST_CHANNELS, TEST_EXPECTED_BUFFER_SIZE);
    
    uint32_t test_samples = TEST_EXPECTED_BUFFER_SIZE;
    float input_buffer[TEST_CHANNELS * test_samples];
    float prefill_buffer[TEST_CHANNELS * TEST_DEPTH]; // Separate buffer for prefill data
    float output_buffer[TEST_CHANNELS * test_samples];
    
    fill_test_data(input_buffer, TEST_CHANNELS, test_samples, 1000.0f);
    
    // Push data
    int ret = pwar_ring_buffer_push(input_buffer, test_samples, TEST_CHANNELS);
    ck_assert_int_eq(ret, 1);
    
    // Check available count increased
    uint32_t expected_available = TEST_DEPTH + test_samples;
    ck_assert_uint_eq(pwar_ring_buffer_get_available(), expected_available);
    
    // Pop prefill data first (should be zeros)
    memset(prefill_buffer, 0xAA, sizeof(prefill_buffer)); // Fill with junk
    ret = pwar_ring_buffer_pop(prefill_buffer, TEST_DEPTH, TEST_CHANNELS);
    ck_assert_int_eq(ret, TEST_DEPTH);
    
    // Verify prefill was zeros
    for (uint32_t i = 0; i < TEST_DEPTH * TEST_CHANNELS; i++) {
        ck_assert_float_eq_tol(prefill_buffer[i], 0.0f, 0.0001f);
    }
    
    // Now pop our test data
    memset(output_buffer, 0xAA, sizeof(output_buffer));
    ret = pwar_ring_buffer_pop(output_buffer, test_samples, TEST_CHANNELS);
    ck_assert_int_eq(ret, test_samples);
    
    verify_test_data(output_buffer, TEST_CHANNELS, test_samples, 1000.0f, "basic_push_pop");
    
    pwar_ring_buffer_free();
}
END_TEST

// Test underrun behavior - this is where bugs often hide!
START_TEST(test_ring_buffer_underrun)
{
    pwar_ring_buffer_init(TEST_DEPTH, TEST_CHANNELS, TEST_EXPECTED_BUFFER_SIZE);
    
    uint32_t initial_underruns = pwar_ring_buffer_get_underruns();
    
    // Try to pop more data than available (including prefill)
    uint32_t excessive_samples = TEST_DEPTH + 100;
    float output_buffer[TEST_CHANNELS * excessive_samples];
    memset(output_buffer, 0xAA, sizeof(output_buffer)); // Fill with junk
    
    int ret = pwar_ring_buffer_pop(output_buffer, excessive_samples, TEST_CHANNELS);
    ck_assert_int_eq(ret, excessive_samples); // Should return requested count
    
    // Verify underrun was detected
    ck_assert_uint_gt(pwar_ring_buffer_get_underruns(), initial_underruns);
    
    // Verify output is all zeros (silence)
    for (uint32_t i = 0; i < excessive_samples * TEST_CHANNELS; i++) {
        ck_assert_float_eq_tol(output_buffer[i], 0.0f, 0.0001f);
    }
    
    // After underrun, buffer should be prefilled again
    uint32_t expected_available = TEST_DEPTH;
    ck_assert_uint_eq(pwar_ring_buffer_get_available(), expected_available);
    
    pwar_ring_buffer_free();
}
END_TEST

// Test multiple small underruns
START_TEST(test_ring_buffer_multiple_underruns)
{
    pwar_ring_buffer_init(TEST_DEPTH, TEST_CHANNELS, TEST_EXPECTED_BUFFER_SIZE);
    
    uint32_t small_chunk = TEST_EXPECTED_BUFFER_SIZE;
    float output_buffer[TEST_CHANNELS * small_chunk];
    
    // Consume all prefill data first
    uint32_t prefill_samples = TEST_DEPTH;
    float prefill_buffer[TEST_CHANNELS * prefill_samples];
    pwar_ring_buffer_pop(prefill_buffer, prefill_samples, TEST_CHANNELS);
    
    uint32_t initial_underruns = pwar_ring_buffer_get_underruns();
    
    // Now trigger multiple underruns
    for (int i = 0; i < 5; i++) {
        memset(output_buffer, 0xAA, sizeof(output_buffer));
        int ret = pwar_ring_buffer_pop(output_buffer, small_chunk, TEST_CHANNELS);
        ck_assert_int_eq(ret, small_chunk);
        
        // Each should be zeros
        for (uint32_t j = 0; j < small_chunk * TEST_CHANNELS; j++) {
            ck_assert_float_eq_tol(output_buffer[j], 0.0f, 0.0001f);
        }
    }
    
    // Should have detected multiple underruns
    ck_assert_uint_gt(pwar_ring_buffer_get_underruns(), initial_underruns);
    
    pwar_ring_buffer_free();
}
END_TEST

// Test overrun behavior - critical for finding bugs!
START_TEST(test_ring_buffer_overrun)
{
    pwar_ring_buffer_init(TEST_DEPTH, TEST_CHANNELS, TEST_EXPECTED_BUFFER_SIZE);
    
    uint32_t initial_overruns = pwar_ring_buffer_get_overruns();
    
    // Fill buffer completely and then some more
    uint32_t excessive_samples = TEST_DEPTH + 500; // More than total capacity
    float input_buffer[TEST_CHANNELS * excessive_samples];
    fill_test_data(input_buffer, TEST_CHANNELS, excessive_samples, 2000.0f);
    
    int ret = pwar_ring_buffer_push(input_buffer, excessive_samples, TEST_CHANNELS);
    ck_assert_int_eq(ret, 1); // Should succeed
    
    // Verify overrun was detected
    ck_assert_uint_gt(pwar_ring_buffer_get_overruns(), initial_overruns);
    
    // Buffer should be full after overrun, and full means TEST_DEPTH + TEST_EXPECTED_BUFFER_SIZE due to safety margin
    ck_assert_uint_eq(pwar_ring_buffer_get_available(), TEST_DEPTH+TEST_EXPECTED_BUFFER_SIZE);
    
    // When we pop data, we should get the LATEST data (oldest was discarded)
    float output_buffer[TEST_CHANNELS * TEST_DEPTH];
    ret = pwar_ring_buffer_pop(output_buffer, TEST_DEPTH, TEST_CHANNELS);
    ck_assert_int_eq(ret, TEST_DEPTH);
    
    // The data should be from the end of the input (newest data)
    // Account for the actual buffer capacity including safety margin
    uint32_t actual_buffer_capacity = TEST_DEPTH + TEST_EXPECTED_BUFFER_SIZE;
    uint32_t offset = excessive_samples - actual_buffer_capacity;
    verify_test_data(output_buffer, TEST_CHANNELS, TEST_DEPTH, 2000.0f + offset, "overrun_latest_data");
    
    pwar_ring_buffer_free();
}
END_TEST

// Test gradual overrun build-up
START_TEST(test_ring_buffer_gradual_overrun)
{
    pwar_ring_buffer_init(TEST_DEPTH, TEST_CHANNELS, TEST_EXPECTED_BUFFER_SIZE);
    
    uint32_t chunk_size = TEST_EXPECTED_BUFFER_SIZE;
    float input_buffer[TEST_CHANNELS * chunk_size];
    
    // Keep pushing without popping to gradually fill buffer
    uint32_t total_pushed = 0;
    for (int i = 0; i < 10; i++) {
        fill_test_data(input_buffer, TEST_CHANNELS, chunk_size, i * 100.0f);
        int ret = pwar_ring_buffer_push(input_buffer, chunk_size, TEST_CHANNELS);
        ck_assert_int_eq(ret, 1);
        total_pushed += chunk_size;
        
        // Check if overrun occurred
        if (total_pushed > TEST_DEPTH) {
            ck_assert_uint_gt(pwar_ring_buffer_get_overruns(), 0);
            break;
        }
    }
    
    // Buffer should be at capacity (including safety margin)
    ck_assert_uint_eq(pwar_ring_buffer_get_available(), TEST_DEPTH + TEST_EXPECTED_BUFFER_SIZE);
    
    pwar_ring_buffer_free();
}
END_TEST

// Test concurrent overrun and underrun scenarios
START_TEST(test_ring_buffer_mixed_overrun_underrun)
{
    pwar_ring_buffer_init(TEST_DEPTH, TEST_CHANNELS, TEST_EXPECTED_BUFFER_SIZE);
    
    // First cause an overrun - need to exceed actual buffer capacity (TEST_DEPTH + TEST_EXPECTED_BUFFER_SIZE)
    uint32_t large_push = TEST_DEPTH + TEST_EXPECTED_BUFFER_SIZE + 100; // 1380 > 1280, will cause overrun
    float input_buffer[TEST_CHANNELS * large_push];
    fill_test_data(input_buffer, TEST_CHANNELS, large_push, 3000.0f);
    pwar_ring_buffer_push(input_buffer, large_push, TEST_CHANNELS);
    
    uint32_t overruns_after_push = pwar_ring_buffer_get_overruns();
    ck_assert_uint_gt(overruns_after_push, 0);
    
    // Now cause an underrun by popping more than available
    // After overrun, buffer has TEST_DEPTH + TEST_EXPECTED_BUFFER_SIZE samples
    uint32_t available_after_overrun = TEST_DEPTH + TEST_EXPECTED_BUFFER_SIZE;
    uint32_t large_pop = available_after_overrun + 100; // Pop more than available
    float output_buffer[TEST_CHANNELS * large_pop];
    pwar_ring_buffer_pop(output_buffer, large_pop, TEST_CHANNELS);
    
    uint32_t underruns_after_pop = pwar_ring_buffer_get_underruns();
    ck_assert_uint_gt(underruns_after_pop, 0);
    
    // After underrun, buffer should be prefilled again
    uint32_t expected_available = TEST_DEPTH;
    ck_assert_uint_eq(pwar_ring_buffer_get_available(), expected_available);
    
    pwar_ring_buffer_free();
}
END_TEST

// Test channel count validation
START_TEST(test_ring_buffer_channel_mismatch)
{
    pwar_ring_buffer_init(TEST_DEPTH, TEST_CHANNELS, TEST_EXPECTED_BUFFER_SIZE);
    
    float buffer[3 * TEST_EXPECTED_BUFFER_SIZE]; // Wrong channel count
    
    // Push with wrong channel count should fail
    int ret = pwar_ring_buffer_push(buffer, TEST_EXPECTED_BUFFER_SIZE, 3);
    ck_assert_int_eq(ret, -1);
    
    // Pop with wrong channel count should fail
    ret = pwar_ring_buffer_pop(buffer, TEST_EXPECTED_BUFFER_SIZE, 3);
    ck_assert_int_eq(ret, -1);
    
    pwar_ring_buffer_free();
}
END_TEST

// Test NULL pointer handling
START_TEST(test_ring_buffer_null_pointers)
{
    pwar_ring_buffer_init(TEST_DEPTH, TEST_CHANNELS, TEST_EXPECTED_BUFFER_SIZE);
    
    float buffer[TEST_CHANNELS * TEST_EXPECTED_BUFFER_SIZE];
    
    // Push with NULL buffer should fail
    int ret = pwar_ring_buffer_push(NULL, TEST_EXPECTED_BUFFER_SIZE, TEST_CHANNELS);
    ck_assert_int_eq(ret, -1);
    
    // Pop with NULL buffer should fail
    ret = pwar_ring_buffer_pop(NULL, TEST_EXPECTED_BUFFER_SIZE, TEST_CHANNELS);
    ck_assert_int_eq(ret, -1);
    
    pwar_ring_buffer_free();
    
    // Operations on uninitialized buffer should fail
    ret = pwar_ring_buffer_push(buffer, TEST_EXPECTED_BUFFER_SIZE, TEST_CHANNELS);
    ck_assert_int_eq(ret, -1);
    
    ret = pwar_ring_buffer_pop(buffer, TEST_EXPECTED_BUFFER_SIZE, TEST_CHANNELS);
    ck_assert_int_eq(ret, -1);
}
END_TEST

// Test statistics reset
START_TEST(test_ring_buffer_stats_reset)
{
    pwar_ring_buffer_init(TEST_DEPTH, TEST_CHANNELS, TEST_EXPECTED_BUFFER_SIZE);
    
    // Cause an overrun first
    float large_buffer[TEST_CHANNELS * (TEST_DEPTH + 100)];
    pwar_ring_buffer_push(large_buffer, TEST_DEPTH + 100, TEST_CHANNELS);
    ck_assert_uint_gt(pwar_ring_buffer_get_overruns(), 0);
    
    // Check how many samples are actually available after the overrun
    uint32_t available_after_overrun = pwar_ring_buffer_get_available();
    
    // Now cause an underrun by popping more than what's available
    uint32_t samples_to_cause_underrun = available_after_overrun + 100;
    float extra_large_buffer[TEST_CHANNELS * samples_to_cause_underrun];
    pwar_ring_buffer_pop(extra_large_buffer, samples_to_cause_underrun, TEST_CHANNELS);
    
    ck_assert_uint_gt(pwar_ring_buffer_get_overruns(), 0);
    ck_assert_uint_gt(pwar_ring_buffer_get_underruns(), 0);
    
    // Reset stats
    pwar_ring_buffer_reset_stats();
    
    ck_assert_uint_eq(pwar_ring_buffer_get_overruns(), 0);
    ck_assert_uint_eq(pwar_ring_buffer_get_underruns(), 0);
    
    pwar_ring_buffer_free();
}
END_TEST

// Test edge case: zero samples
START_TEST(test_ring_buffer_zero_samples)
{
    pwar_ring_buffer_init(TEST_DEPTH, TEST_CHANNELS, TEST_EXPECTED_BUFFER_SIZE);
    
    float buffer[TEST_CHANNELS];
    
    // Push zero samples should succeed but do nothing
    int ret = pwar_ring_buffer_push(buffer, 0, TEST_CHANNELS);
    ck_assert_int_eq(ret, 1);
    
    uint32_t available_before = pwar_ring_buffer_get_available();
    
    // Pop zero samples should succeed but do nothing
    ret = pwar_ring_buffer_pop(buffer, 0, TEST_CHANNELS);
    ck_assert_int_eq(ret, 0);
    
    ck_assert_uint_eq(pwar_ring_buffer_get_available(), available_before);
    
    pwar_ring_buffer_free();
}
END_TEST

// Test re-initialization (free and init again)
START_TEST(test_ring_buffer_reinit)
{
    // First initialization
    pwar_ring_buffer_init(TEST_DEPTH, TEST_CHANNELS, TEST_EXPECTED_BUFFER_SIZE);
    
    float buffer[TEST_CHANNELS * TEST_EXPECTED_BUFFER_SIZE];
    fill_test_data(buffer, TEST_CHANNELS, TEST_EXPECTED_BUFFER_SIZE, 1000.0f);
    pwar_ring_buffer_push(buffer, TEST_EXPECTED_BUFFER_SIZE, TEST_CHANNELS);
    
    // Re-initialize with different parameters
    pwar_ring_buffer_init(TEST_DEPTH/2, TEST_CHANNELS, TEST_EXPECTED_BUFFER_SIZE/2);
    
    // Should have new prefill amount
    uint32_t expected_available = TEST_DEPTH/2;
    ck_assert_uint_eq(pwar_ring_buffer_get_available(), expected_available);
    
    // Stats should be reset
    ck_assert_uint_eq(pwar_ring_buffer_get_overruns(), 0);
    ck_assert_uint_eq(pwar_ring_buffer_get_underruns(), 0);
    
    pwar_ring_buffer_free();
}
END_TEST

// Critical test: Verify wrap-around behavior doesn't corrupt data
START_TEST(test_ring_buffer_wrap_around_integrity)
{
    // Use smaller buffer to force wrap-around quickly
    uint32_t small_depth = 100;
    uint32_t small_expected = 20;
    pwar_ring_buffer_init(small_depth, TEST_CHANNELS, small_expected);
    
    uint32_t chunk_size = small_expected; // Use expected buffer size for this test
    float input_buffer[TEST_CHANNELS * chunk_size];
    float output_buffer[TEST_CHANNELS * chunk_size];
    
    // Consume prefill first
    float prefill_buffer[TEST_CHANNELS * small_depth];
    pwar_ring_buffer_pop(prefill_buffer, small_depth, TEST_CHANNELS);
    
    // Now do multiple push/pop cycles to force wrap-around
    for (int cycle = 0; cycle < 10; cycle++) {
        fill_test_data(input_buffer, TEST_CHANNELS, chunk_size, cycle * 1000.0f);
        
        int ret = pwar_ring_buffer_push(input_buffer, chunk_size, TEST_CHANNELS);
        ck_assert_int_eq(ret, 1);
        
        memset(output_buffer, 0xAA, sizeof(output_buffer));
        ret = pwar_ring_buffer_pop(output_buffer, chunk_size, TEST_CHANNELS);
        ck_assert_int_eq(ret, chunk_size);
        
        verify_test_data(output_buffer, TEST_CHANNELS, chunk_size, cycle * 1000.0f, "wrap_around");
    }
    
    pwar_ring_buffer_free();
}
END_TEST

// Test suite setup
Suite *ring_buffer_suite(void) {
    Suite *s = suite_create("pwar_ring_buffer");
    
    TCase *tc_core = tcase_create("Core");
    tcase_add_test(tc_core, test_ring_buffer_init_free);
    tcase_add_test(tc_core, test_ring_buffer_basic_push_pop);
    tcase_add_test(tc_core, test_ring_buffer_channel_mismatch);
    tcase_add_test(tc_core, test_ring_buffer_null_pointers);
    tcase_add_test(tc_core, test_ring_buffer_zero_samples);
    tcase_add_test(tc_core, test_ring_buffer_reinit);
    tcase_add_test(tc_core, test_ring_buffer_stats_reset);
    suite_add_tcase(s, tc_core);
    
    TCase *tc_underrun = tcase_create("Underrun");
    tcase_add_test(tc_underrun, test_ring_buffer_underrun);
    tcase_add_test(tc_underrun, test_ring_buffer_multiple_underruns);
    suite_add_tcase(s, tc_underrun);
    
    TCase *tc_overrun = tcase_create("Overrun");
    tcase_add_test(tc_overrun, test_ring_buffer_overrun);
    tcase_add_test(tc_overrun, test_ring_buffer_gradual_overrun);
    suite_add_tcase(s, tc_overrun);
    
    TCase *tc_edge_cases = tcase_create("EdgeCases");
    tcase_add_test(tc_edge_cases, test_ring_buffer_mixed_overrun_underrun);
    tcase_add_test(tc_edge_cases, test_ring_buffer_wrap_around_integrity);
    suite_add_tcase(s, tc_edge_cases);
    
    return s;
}

// Main entry for running the test suite
int main(void) {
    int number_failed;
    Suite *s = ring_buffer_suite();
    SRunner *sr = srunner_create(s);
    
    // Run all tests
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    
    return (number_failed == 0) ? 0 : 1;
}
