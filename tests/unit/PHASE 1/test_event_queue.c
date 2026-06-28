#include "unity.h"
#include "event_queue.h"

// Runs before every single test
void setUp(void) {
    eventQueue_init();
}

// Runs after every single test
void tearDown(void) {}

/* ==================================================================== */
/* TEST CASES                                                           */
/* ==================================================================== */

void test_eventQueue_init_StartsEmpty(void) {
    event_t dummy;
    event_status_t status = eventQueue_poll(&dummy);
    
    TEST_ASSERT_EQUAL(EVENT_QUEUE_EMPTY, status);
}

void test_eventQueue_post_and_poll_SingleEvent(void) {
    event_t evt_in  = { .event_id = 5, .timestamp = 1000, .param1 = 0xAA, .param2 = 0xBB };
    event_t evt_out = { 0 };
    
    // Post the event
    TEST_ASSERT_EQUAL(EVENT_QUEUE_OK, eventQueue_post(&evt_in));
    
    // Poll the event
    TEST_ASSERT_EQUAL(EVENT_QUEUE_OK, eventQueue_poll(&evt_out));
    
    // Verify the envelope remained intact
    TEST_ASSERT_EQUAL(5, evt_out.event_id);
    TEST_ASSERT_EQUAL(1000, evt_out.timestamp);
    TEST_ASSERT_EQUAL(0xAA, evt_out.param1);
}

void test_eventQueue_ReturnsFull_WhenCapacityReached(void) {
    event_t dummy = { .event_id = 1 };
    
    // The usable capacity of a ring buffer is (DEPTH - 1)
    int max_capacity = EVENT_QUEUE_DEPTH - 1; 
    
    // Fill the queue to the brim
    for (int i = 0; i < max_capacity; i++) {
        TEST_ASSERT_EQUAL(EVENT_QUEUE_OK, eventQueue_post(&dummy));
    }
    
    // The very next post must fail
    TEST_ASSERT_EQUAL(EVENT_QUEUE_FULL, eventQueue_post(&dummy));
}

void test_eventQueue_WrapAround_Succeeds(void) {
    event_t in_evt = { .event_id = 99 };
    event_t out_evt;
    
    // We intentionally post and poll more times than the depth of the queue
    // to force the pointers to wrap around the end of the array.
    for (int i = 0; i < (EVENT_QUEUE_DEPTH * 3); i++) {
        TEST_ASSERT_EQUAL(EVENT_QUEUE_OK, eventQueue_post(&in_evt));
        TEST_ASSERT_EQUAL(EVENT_QUEUE_OK, eventQueue_poll(&out_evt));
        
        // Assert we actually got our data back
        TEST_ASSERT_EQUAL(99, out_evt.event_id);
    }
}

// The main test runner
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_eventQueue_init_StartsEmpty);
    RUN_TEST(test_eventQueue_post_and_poll_SingleEvent);
    RUN_TEST(test_eventQueue_ReturnsFull_WhenCapacityReached);
    RUN_TEST(test_eventQueue_WrapAround_Succeeds);
    return UNITY_END();
}