# Event Queue Middleware --- Phase 1

**Project:** Automotive / Industrial Telematics Gateway\
**Phase:** 1 --- Event Queue and Middleware Foundation\
**Language:** C\
**Target:** STM32 / ARM Cortex-M firmware\
**Verification:** Native desktop software-level testing\
**Test framework:** Unity C-style test interface with a lightweight
native test runner\
**Status:** Phase completed and verified at software level

------------------------------------------------------------------------

## 1. Purpose

Phase 1 establishes a small, deterministic communication mechanism
between asynchronous firmware components and application logic.

At this stage the event queue is **middleware**. It does not depend on
UART, CAN, SPI, DMA, GPIO, or another peripheral.

The intended architecture is:

``` text
Future drivers / interrupt sources
              |
              v
        Event Queue
              |
              v
         Dispatcher
              |
              v
       Application logic
```

The guiding principle is:

> **Interrupts should report that something happened; application logic
> decides what to do about it.**

------------------------------------------------------------------------

## 2. Why Build the Queue Before Peripheral Drivers?

The queue's core responsibilities are software properties:

-   store events,
-   preserve event order,
-   detect empty state,
-   detect full state,
-   advance read/write positions,
-   wrap indexes correctly.

The public interface is hardware-independent:

``` c
event_status_t eventQueue_init(void);
event_status_t eventQueue_post(const event_t *p_event);
event_status_t eventQueue_poll(event_t *p_event);
```

Therefore the queue could be verified before UART, CAN, SPI, DMA, or
interrupt-driver integration.

The intended verification progression was:

``` text
Event Queue Design
        |
        v
Software-Level Verification
        |
        v
Firmware / Driver Integration
        |
        v
Hardware Validation
        |
        v
HIL / System Validation
```

The reason is diagnostic isolation. If the queue is first tested only
after hardware integration, a failure could originate from the queue,
driver, interrupt timing, peripheral configuration, or integration
logic. Testing the middleware independently reduces that search space.

------------------------------------------------------------------------

## 3. Event Representation

The event envelope is:

``` c
typedef struct {
    uint8_t  event_id;
    uint32_t timestamp;
    uint32_t param1;
    uint32_t param2;
} event_t;
```

The queue transports this structure without needing to understand the
meaning of its fields.

-   `event_id` --- event type
-   `timestamp` --- timing information
-   `param1` / `param2` --- event-specific parameters

This keeps event transport separate from application semantics.

------------------------------------------------------------------------

## 4. Queue Architecture

The queue is a statically allocated ring buffer:

``` c
static event_t  s_queue[EVENT_QUEUE_DEPTH];
static uint16_t s_head = 0;
static uint16_t s_tail = 0;
```

The configured depth is:

``` c
#define EVENT_QUEUE_DEPTH 32
```

The implementation uses one reserved slot to distinguish full from
empty.

Therefore:

``` text
Physical storage: 32 slots
Usable capacity:  31 events
```

This is explicitly reflected in the test suite:

``` c
int max_capacity = EVENT_QUEUE_DEPTH - 1;
```

------------------------------------------------------------------------

## 5. Head / Tail Model

Conceptually:

``` text
s_head -> next write position
s_tail -> next read position
```

The queue is empty when:

``` c
s_head == s_tail
```

The next head position is calculated before a write:

``` c
uint16_t next_head =
    (s_head + 1) & (EVENT_QUEUE_DEPTH - 1);
```

If the next head would collide with the tail, the queue is full.

------------------------------------------------------------------------

## 6. Why a Power-of-Two Depth?

`EVENT_QUEUE_DEPTH` is 32, a power of two.

That permits index wrapping with a bit mask:

``` c
index & (EVENT_QUEUE_DEPTH - 1)
```

instead of modulo:

``` c
index % EVENT_QUEUE_DEPTH
```

For example:

``` c
s_head = (s_head + 1) & (EVENT_QUEUE_DEPTH - 1);
s_tail = (s_tail + 1) & (EVENT_QUEUE_DEPTH - 1);
```

For a depth of 32:

``` text
32 - 1 = 31 = 0b00011111
```

The mask therefore keeps the index within `0...31`.

**Design constraint:** this wrapping technique depends on the queue
depth being a power of two. Changing the depth to a non-power-of-two
value would require a different wrap-around method.

------------------------------------------------------------------------

## 7. Posting an Event

The implementation protects the queue operation with a short
interrupt-critical section:

``` c
uint32_t primask_state = __get_PRIMASK();
__disable_irq();

uint16_t next_head =
    (s_head + 1) & (EVENT_QUEUE_DEPTH - 1);

if (next_head == s_tail) {
    __set_PRIMASK(primask_state);
    return EVENT_QUEUE_FULL;
}

s_queue[s_head] = *p_event;
s_head = next_head;

__set_PRIMASK(primask_state);
return EVENT_QUEUE_OK;
```

The sequence is:

1.  Save interrupt state.
2.  Disable interrupts.
3.  Calculate next head.
4.  Check full condition.
5.  Copy the event.
6.  Advance the head.
7.  Restore the previous interrupt state.
8.  Return the result.

Restoring the previous state is preferable to unconditionally enabling
interrupts because it preserves the caller's prior interrupt
configuration.

------------------------------------------------------------------------

## 8. Polling an Event

The polling path follows the same short critical-section model:

``` c
if (s_head == s_tail) {
    __set_PRIMASK(primask_state);
    return EVENT_QUEUE_EMPTY;
}

*p_event = s_queue[s_tail];
s_tail = (s_tail + 1) & (EVENT_QUEUE_DEPTH - 1);
```

This gives FIFO behaviour:

``` text
First posted event
        |
        v
First event returned
```

------------------------------------------------------------------------

## 9. Desktop Test Build

The implementation contains a desktop-test compilation path:

``` c
#ifdef DEKTOP_TESTING
```

In that build, the MCU-specific interrupt functions are replaced with
no-op stubs:

``` c
static inline uint32_t __get_PRIMASK(void) {
    return 0;
}

static inline void __disable_irq(void) {
    /* Left intentionally blank for Windows PC execution */
}

static inline void __set_PRIMASK(uint32_t priMask) {
    (void)priMask;
}
```

The target build instead includes the STM32/system headers.

The important idea is that the **queue algorithm remains the same**
while the hardware-specific interrupt-control layer is substituted for
host execution.

------------------------------------------------------------------------

## 10. Why Software-Level Unit Testing?

The first test stage was not intended to prove that the STM32 system
worked.

It answered:

> **Does the event queue implementation behave correctly as software?**

That is different from HIL.

### Software-level testing

``` text
Is the queue algorithm correct?
```

### HIL testing

``` text
Does the integrated firmware behave correctly
on the actual target hardware?
```

These are complementary.

The first removes hardware complexity. The second introduces real
hardware and integration behaviour.

------------------------------------------------------------------------

## 11. Unity Test Interface and Native Runner

The project uses a Unity C-style interface:

``` c
#define UNITY_BEGIN() UnityBegin(__FILE__)
#define UNITY_END()   UnityEnd()
#define RUN_TEST(func) UnityDefaultTestRun(func, #func, __LINE__)
#define TEST_ASSERT_EQUAL(expected, actual) \
    UnityAssertEqualNumber((int32_t)(expected), \
                           (int32_t)(actual), \
                           NULL, \
                           __LINE__)
```

The test project also contains a lightweight test engine implementation
that counts tests and failures and reports pass/fail results.

The native runner is:

``` c
int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_eventQueue_init_StartsEmpty);
    RUN_TEST(test_eventQueue_post_and_poll_SingleEvent);
    RUN_TEST(test_eventQueue_ReturnsFull_WhenCapacityReached);
    RUN_TEST(test_eventQueue_WrapAround_Succeeds);

    return UNITY_END();
}
```

This allowed the event queue to be compiled and executed as a desktop
test program rather than flashed to the MCU.

------------------------------------------------------------------------

## 12. Test Isolation

Every test starts with:

``` c
void setUp(void) {
    eventQueue_init();
}
```

This resets the queue before each test so that one test does not depend
on state left by another.

The current teardown is empty:

``` c
void tearDown(void) {}
```

because the queue uses static storage and no additional cleanup is
required by the current implementation.

------------------------------------------------------------------------

## 13. Test Cases

### 13.1 Initialization / Empty State

``` c
void test_eventQueue_init_StartsEmpty(void) {
    event_t dummy;
    event_status_t status = eventQueue_poll(&dummy);

    TEST_ASSERT_EQUAL(EVENT_QUEUE_EMPTY, status);
}
```

Purpose:

Verify that initialization establishes an empty queue.

------------------------------------------------------------------------

### 13.2 Single Event Post / Poll

``` c
event_t evt_in = {
    .event_id = 5,
    .timestamp = 1000,
    .param1 = 0xAA,
    .param2 = 0xBB
};
```

The test posts the event, polls it, and verifies that event data is
preserved.

This checks both the queue operation and the event envelope transfer.

------------------------------------------------------------------------

### 13.3 Full Queue

The test fills the usable capacity:

``` c
int max_capacity = EVENT_QUEUE_DEPTH - 1;

for (int i = 0; i < max_capacity; i++) {
    TEST_ASSERT_EQUAL(EVENT_QUEUE_OK,
                      eventQueue_post(&dummy));
}
```

Then verifies:

``` c
TEST_ASSERT_EQUAL(EVENT_QUEUE_FULL,
                  eventQueue_post(&dummy));
```

This explicitly checks the capacity boundary.

------------------------------------------------------------------------

### 13.4 Wrap-Around

The test repeatedly posts and polls:

``` c
for (int i = 0; i < (EVENT_QUEUE_DEPTH * 3); i++) {
    TEST_ASSERT_EQUAL(EVENT_QUEUE_OK,
                      eventQueue_post(&in_evt));

    TEST_ASSERT_EQUAL(EVENT_QUEUE_OK,
                      eventQueue_poll(&out_evt));

    TEST_ASSERT_EQUAL(99, out_evt.event_id);
}
```

The purpose is to exercise repeated pointer wrapping.

------------------------------------------------------------------------

## 14. What Was Actually Verified?

The current test source provides evidence for:

-   initialization,
-   empty-state detection,
-   event posting,
-   event polling,
-   event data transfer,
-   queue-full detection,
-   repeated wrap-around behaviour,
-   basic FIFO behaviour through post/poll sequencing.

It does **not** by itself prove:

-   STM32 interrupt timing,
-   concurrent producer/consumer behaviour on the MCU,
-   UART/CAN/SPI driver correctness,
-   DMA behaviour,
-   peripheral configuration,
-   electrical behaviour,
-   complete system timing,
-   HIL behaviour.

Those belong to later verification stages.

> **A passing unit test establishes confidence only in the behaviour
> actually exercised by the test suite.**

------------------------------------------------------------------------

## 15. Could Another Testing Mechanism Have Been Used?

Yes.

Unity was a tool choice, not the fundamental engineering decision.

Possible approaches include:

### Manual native C tests

A simple `main()` could call the queue and check results manually.

Simple, but harder to scale and report consistently.

### Ceedling + Unity

Could automate build and test execution as the project grows.

### GoogleTest

Could be appropriate for a C++-oriented test environment, but would
introduce a heavier C++ test stack for this C middleware.

### Python / pytest

The native queue could potentially be exposed through a host library and
tested from Python.

### Property-based testing / fuzzing

Could generate many post/poll sequences and verify queue invariants
automatically.

The important principle is:

> **The framework is secondary to the verification strategy.**

The actual strategy here was to isolate the middleware and verify it
before hardware integration.

------------------------------------------------------------------------

## 16. Why Unity Was Reasonable for This Phase

The required operations were small and deterministic:

``` text
Initialize
    ↓
Post
    ↓
Poll
    ↓
Assert
```

The lightweight Unity-style interface provided:

-   named tests,
-   assertions,
-   setup,
-   test counting,
-   failure reporting,
-   a native executable.

That was enough for the scope of this phase without introducing a large
testing infrastructure.

------------------------------------------------------------------------

# 17. Architecture Decision Record

## ADR-001 --- Verify Middleware Before Peripheral Integration

**Status:** Implemented

### Context

The event queue was intended to become a communication backbone between
future drivers and application logic.

Its fundamental behaviour did not require an STM32 peripheral.

### Decision

Validate the queue with a native desktop test harness before relying on
it as part of the integrated embedded system.

### Reason

The queue is hardware-independent middleware. Testing it in isolation
reduces the number of variables involved and establishes confidence in
one architectural layer before adding peripheral and hardware
complexity.

### Alternatives considered

1.  Test only after STM32 integration.
2.  Use manual `printf()` checks.
3.  Skip unit testing and rely on HIL.
4.  Adopt a larger automated testing framework.

### Trade-off

A separate native test build and test harness are required.

### Outcome

The queue's core behaviours were exercised independently before later
hardware/system validation.

------------------------------------------------------------------------

# 18. Engineering Questions and Answers

### Why did I test the queue before using an MCU driver?

Because the queue itself was middleware and its fundamental behaviour
was hardware-independent. There was no reason to introduce UART, CAN,
SPI, or another peripheral merely to determine whether FIFO storage,
empty/full detection, and pointer wrapping worked correctly.

### Was I testing the STM32?

No. The first stage was software-level validation of the queue
middleware.

### Why not wait for HIL?

Waiting would mix queue defects with integration and hardware defects.
Testing the queue first reduced the debugging search space.

### What exactly did Unity prove?

Only the behaviours exercised by the test suite: initialization, event
transfer, full detection, and wrap-around, plus the tested post/poll
ordering.

### Could I have used another framework?

Yes. Unity was one implementation choice. The deeper decision was to
verify a hardware-independent architectural layer in isolation.

### Why is HIL still necessary?

Because software-level tests cannot prove the behaviour of the complete
embedded system under real hardware, timing, interrupt, and integration
conditions.

### What was the main engineering decision?

**Separate middleware verification from hardware integration.**

------------------------------------------------------------------------

# 19. Lessons Learned

## 19.1 Verify the layer you can isolate

If a component can be tested without hardware, there is value in doing
so before adding hardware complexity.

## 19.2 Test boundaries

The queue is not only tested in its normal state. It is tested when:

-   empty,
-   full,
-   wrapping,
-   transferring event data.

## 19.3 Preserve the reasoning

The tool name is easy to remember:

> "I used Unity."

The engineering rationale is easier to forget:

> "I used host-based unit testing because the queue was middleware and
> could be verified independently before peripheral integration."

That rationale should be preserved in project documentation.

## 19.4 Unit tests and HIL are different layers

They should not be treated as competing methods.

``` text
Unit / host testing
        |
        v
Software confidence
        |
        v
Target integration
        |
        v
HIL
        |
        v
System confidence
```

------------------------------------------------------------------------

# 20. Limitations and Future Improvements

The current suite can be extended with:

-   multiple distinct events to test stronger FIFO ordering,
-   explicit verification of `param2`,
-   repeated full/empty transitions,
-   larger mixed post/poll sequences,
-   boundary tests immediately around wrap-around,
-   invalid/null pointer tests where appropriate,
-   target-side concurrency tests,
-   interrupt-context integration tests,
-   timing/latency measurements,
-   property-based or fuzz testing.

These are **future opportunities**, not claims about tests already
performed.

------------------------------------------------------------------------

# 21. Phase Boundary

The important question after this phase became:

> **How should real asynchronous hardware events enter the
> architecture?**

The next stage can therefore connect the verified middleware to actual
drivers:

``` text
UART / CAN / SPI / GPIO / Timer
              |
              v
         Driver layer
              |
              v
         Event Queue
              |
              v
          Dispatcher
              |
              v
        Application
```

The queue is intended to remain infrastructure rather than becoming
responsible for application-specific decisions.

------------------------------------------------------------------------

# 22. Files in This Phase

``` text
event_queue.h
event_queue.c
test_event_queue.c
unity.h
unity.c
test_runner.exe
```

The source files document the implementation and the host-side
verification approach.

------------------------------------------------------------------------

# 23. Final Engineering Record

**Decision:** Build and verify the event queue as middleware before
peripheral integration.

**Why:** Its core behaviour was hardware-independent and could be tested
in isolation.

**Verification mechanism:** Native desktop testing using the project's
Unity C-style test interface and lightweight test runner.

**Verified behaviours:**

-   initialization,
-   empty detection,
-   event post/poll,
-   event data preservation,
-   queue-full detection,
-   ring-buffer wrap-around.

**Later verification:** Hardware-in-the-Loop testing was used as a
separate stage for integrated target validation.

**Core principle:**

> **Establish confidence in each architectural layer before introducing
> the complexity of the next layer.**
