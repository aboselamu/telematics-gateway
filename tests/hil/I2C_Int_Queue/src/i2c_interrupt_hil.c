#include "i2c_interrupt_hil.h"

#include <stddef.h>


/*=============================================================================
 * HIL Configuration
 *============================================================================*/

#define DS3231_I2C_ADDRESS           0x68U
#define DS3231_REG_SECONDS           0x00U

/*
 * Must be unused on the HIL rig.
 * Change this if another device exists at 0x69.
 */
#define I2C_HIL_NACK_ADDRESS         0x69U

#define I2C_HIL_STRESS_ITERATIONS    10000U


/*=============================================================================
 * Private HIL State
 *============================================================================*/

static i2c_hil_report_t s_report;

static uint8_t s_register_address;
static uint8_t s_rx_buffer[7];

static bool s_started;
static bool s_launch_pending;
static bool s_busy_rejection_ok;

static volatile uint32_t s_event_post_failures;


/*=============================================================================
 * Private Helpers
 *============================================================================*/

static void i2c_hil_schedule(
    i2c_hil_state_t next_state)
{
    s_report.state = next_state;
    s_launch_pending = true;
}


static void i2c_hil_pass(
    i2c_hil_state_t next_state)
{
    s_report.tests_passed++;

    i2c_hil_schedule(next_state);
}


static void i2c_hil_fail(
    i2c_status_t result)
{
    s_report.tests_failed++;
    s_report.last_result = result;
    s_report.state = I2C_HIL_FAILED;

    s_launch_pending = false;
}


static void i2c_hil_clear_rx_buffer(void)
{
    uint32_t i;

    for (i = 0U; i < sizeof(s_rx_buffer); i++)
    {
        s_rx_buffer[i] = 0xA5U;
    }
}


/*=============================================================================
 * I2C Completion Callback
 *
 * Runs in I2C interrupt context.
 *
 * DO NOT:
 * - print
 * - parse RTC data
 * - start another I2C transaction
 * - perform lengthy processing
 *
 * Only convert driver completion into a middleware event.
 *============================================================================*/

static void i2c_hil_callback(
    i2c_status_t result)
{
    event_t event = {0};

    event.event_id = EVT_I2C_TRANSFER_DONE;
    event.param1   = (uint32_t)result;

    if (eventQueue_post(&event) != EVENT_QUEUE_OK)
    {
        s_event_post_failures++;
    }
}


/*=============================================================================
 * Public Initialisation
 *============================================================================*/

void i2c_interrupt_hil_init(void)
{
    s_report.tests_passed = 0U;
    s_report.tests_failed = 0U;

    s_report.stress_completed = 0U;
    s_report.event_post_failures = 0U;

    s_report.last_result = I2C_OK;
    s_report.state = I2C_HIL_IDLE;

    s_register_address = DS3231_REG_SECONDS;

    i2c_hil_clear_rx_buffer();

    s_started = false;
    s_launch_pending = false;
    s_busy_rejection_ok = false;

    s_event_post_failures = 0U;

    /*
     * Base I2C peripheral configuration must already
     * have been completed before this function.
     */
    i2c_it_init();

    /*
     * i2c_it_init() clears the callback, therefore
     * registration MUST occur afterwards.
     */
    i2c_it_register_callback(
        i2c_hil_callback);
}


/*=============================================================================
 * Start Test Sequence
 *============================================================================*/

void i2c_interrupt_hil_start(void)
{
    if (s_started)
    {
        return;
    }

    s_started = true;

    i2c_hil_schedule(
        I2C_HIL_READ_1_BYTE);
}


/*=============================================================================
 * HIL Scheduler
 *
 * Called from main/super-loop context.
 *
 * It launches transactions but never waits for them.
 *============================================================================*/

void i2c_interrupt_hil_process(void)
{
    i2c_status_t status;
    i2c_status_t second_status;

    if (!s_started)
    {
        return;
    }

    /*
     * Detect a lost completion event caused by
     * event queue overflow.
     */
    if (s_event_post_failures !=
        s_report.event_post_failures)
    {
        s_report.event_post_failures =
            s_event_post_failures;

        i2c_hil_fail(
            i2c_it_get_result());

        return;
    }

    if (!s_launch_pending)
    {
        return;
    }

    if ((s_report.state == I2C_HIL_DONE) ||
        (s_report.state == I2C_HIL_FAILED))
    {
        return;
    }

    /*
     * Consume launch request.
     *
     * If the physical bus has not yet released,
     * I2C_ERR_BUS causes us to retry later.
     */
    s_launch_pending = false;


    switch (s_report.state)
    {
        /*----------------------------------------------------------
         * Test 1: combined write-read, receive one byte
         *---------------------------------------------------------*/
        case I2C_HIL_READ_1_BYTE:

            i2c_hil_clear_rx_buffer();

            status = i2c_write_read_it(
                DS3231_I2C_ADDRESS,
                &s_register_address,
                1U,
                s_rx_buffer,
                1U);

            break;


        /*----------------------------------------------------------
         * Test 2: combined write-read, receive two bytes
         *---------------------------------------------------------*/
        case I2C_HIL_READ_2_BYTES:

            i2c_hil_clear_rx_buffer();

            status = i2c_write_read_it(
                DS3231_I2C_ADDRESS,
                &s_register_address,
                1U,
                s_rx_buffer,
                2U);

            break;


        /*----------------------------------------------------------
         * Test 3: combined write-read, receive seven bytes
         *---------------------------------------------------------*/
        case I2C_HIL_READ_7_BYTES:

            i2c_hil_clear_rx_buffer();

            status = i2c_write_read_it(
                DS3231_I2C_ADDRESS,
                &s_register_address,
                1U,
                s_rx_buffer,
                7U);

            break;


        /*----------------------------------------------------------
         * Test 4a: standalone write
         *
         * Only writes the DS3231 register pointer.
         * It does NOT modify RTC contents.
         *---------------------------------------------------------*/
        case I2C_HIL_SEPARATE_WRITE:

            status = i2c_write_it(
                DS3231_I2C_ADDRESS,
                &s_register_address,
                1U);

            break;


        /*----------------------------------------------------------
         * Test 4b: standalone read
         *---------------------------------------------------------*/
        case I2C_HIL_SEPARATE_READ:

            i2c_hil_clear_rx_buffer();

            status = i2c_read_it(
                DS3231_I2C_ADDRESS,
                s_rx_buffer,
                7U);

            break;


        /*----------------------------------------------------------
         * Test 5: synchronous parameter admission
         *
         * No completion event should be generated.
         *---------------------------------------------------------*/
        case I2C_HIL_PARAMETER_TEST:

            if (i2c_read_it(
                    DS3231_I2C_ADDRESS,
                    NULL,
                    1U) != I2C_ERR_PARAM)
            {
                i2c_hil_fail(I2C_ERR_PARAM);
                return;
            }

            if (i2c_write_it(
                    DS3231_I2C_ADDRESS,
                    &s_register_address,
                    0U) != I2C_ERR_PARAM)
            {
                i2c_hil_fail(I2C_ERR_PARAM);
                return;
            }

            if (i2c_write_read_it(
                    0x80U,
                    &s_register_address,
                    1U,
                    s_rx_buffer,
                    1U) != I2C_ERR_PARAM)
            {
                i2c_hil_fail(I2C_ERR_PARAM);
                return;
            }

            /*
             * Parameter test completes synchronously.
             */
            i2c_hil_pass(
                I2C_HIL_BUSY_TEST);

            return;


        /*----------------------------------------------------------
         * Test 6: software ownership / BUSY rejection
         *---------------------------------------------------------*/
        case I2C_HIL_BUSY_TEST:

            i2c_hil_clear_rx_buffer();

            status = i2c_write_read_it(
                DS3231_I2C_ADDRESS,
                &s_register_address,
                1U,
                s_rx_buffer,
                7U);

            if (status == I2C_ERR_BUS)
            {
                s_launch_pending = true;
                return;
            }

            if (status != I2C_OK)
            {
                i2c_hil_fail(status);
                return;
            }

            /*
             * First transaction owns the driver now.
             *
             * This second call MUST be rejected.
             */
            second_status = i2c_read_it(
                DS3231_I2C_ADDRESS,
                s_rx_buffer,
                1U);

            s_busy_rejection_ok =
                (second_status == I2C_ERR_BUSY);

            /*
             * First transaction is still active.
             * Wait for its completion event.
             */
            return;


        /*----------------------------------------------------------
         * Test 7: address NACK
         *---------------------------------------------------------*/
        case I2C_HIL_NACK_TEST:

            status = i2c_write_read_it(
                I2C_HIL_NACK_ADDRESS,
                &s_register_address,
                1U,
                s_rx_buffer,
                1U);

            break;


        /*----------------------------------------------------------
         * Test 8: 10,000 asynchronous combined transfers
         *---------------------------------------------------------*/
        case I2C_HIL_STRESS_TEST:

            status = i2c_write_read_it(
                DS3231_I2C_ADDRESS,
                &s_register_address,
                1U,
                s_rx_buffer,
                7U);

            break;


        default:
            return;
    }


    /*
     * Physical STOP may still be completing from
     * the previous transaction.
     *
     * Retry from a later super-loop iteration.
     */
    if (status == I2C_ERR_BUS)
    {
        s_launch_pending = true;
        return;
    }


    /*
     * Any other immediate failure means the
     * transaction was never accepted.
     */
    if (status != I2C_OK)
    {
        i2c_hil_fail(status);
        return;
    }


    /*
     * Transaction successfully accepted.
     *
     * Nothing else happens here.
     * Completion comes asynchronously through:
     *
     * ISR -> callback -> event queue.
     */
}


/*=============================================================================
 * Event Handler
 *
 * Called from main context after eventQueue_poll().
 *============================================================================*/

void i2c_interrupt_hil_handle_event(
    const event_t *p_event)
{
    i2c_status_t result;

    if (p_event == NULL)
    {
        return;
    }

    if (p_event->event_id !=
        EVT_I2C_TRANSFER_DONE)
    {
        return;
    }

    result =
        (i2c_status_t)p_event->param1;

    s_report.last_result = result;


    switch (s_report.state)
    {
        /*----------------------------------------------------------
         * Tests expected to complete successfully
         *---------------------------------------------------------*/
        case I2C_HIL_READ_1_BYTE:

            if ((result != I2C_OK) ||
                (i2c_it_get_state() != I2C_STATE_READY))
            {
                i2c_hil_fail(result);
                return;
            }

            i2c_hil_pass(
                I2C_HIL_READ_2_BYTES);

            break;


        case I2C_HIL_READ_2_BYTES:

            if ((result != I2C_OK) ||
                (i2c_it_get_state() != I2C_STATE_READY))
            {
                i2c_hil_fail(result);
                return;
            }

            i2c_hil_pass(
                I2C_HIL_READ_7_BYTES);

            break;


        case I2C_HIL_READ_7_BYTES:

            if ((result != I2C_OK) ||
                (i2c_it_get_state() != I2C_STATE_READY))
            {
                i2c_hil_fail(result);
                return;
            }

            i2c_hil_pass(
                I2C_HIL_SEPARATE_WRITE);

            break;


        case I2C_HIL_SEPARATE_WRITE:

            if (result != I2C_OK)
            {
                i2c_hil_fail(result);
                return;
            }

            i2c_hil_pass(
                I2C_HIL_SEPARATE_READ);

            break;


        case I2C_HIL_SEPARATE_READ:

            if (result != I2C_OK)
            {
                i2c_hil_fail(result);
                return;
            }

            i2c_hil_pass(
                I2C_HIL_PARAMETER_TEST);

            break;


        /*----------------------------------------------------------
         * BUSY ownership test
         *---------------------------------------------------------*/
        case I2C_HIL_BUSY_TEST:

            if ((result != I2C_OK) ||
                (!s_busy_rejection_ok))
            {
                i2c_hil_fail(result);
                return;
            }

            i2c_hil_pass(
                I2C_HIL_NACK_TEST);

            break;


        /*----------------------------------------------------------
         * NACK is EXPECTED here
         *---------------------------------------------------------*/
        case I2C_HIL_NACK_TEST:

            if ((result != I2C_ERR_NACK) ||
                (i2c_it_get_state() != I2C_STATE_READY))
            {
                i2c_hil_fail(result);
                return;
            }

            i2c_hil_pass(
                I2C_HIL_STRESS_TEST);

            break;


        /*----------------------------------------------------------
         * Stress test
         *---------------------------------------------------------*/
        case I2C_HIL_STRESS_TEST:

            if (result != I2C_OK)
            {
                i2c_hil_fail(result);
                return;
            }

            s_report.stress_completed++;

            if (s_report.stress_completed >=
                I2C_HIL_STRESS_ITERATIONS)
            {
                s_report.tests_passed++;
                s_report.state =
                    I2C_HIL_DONE;

                s_launch_pending = false;

                return;
            }

            /*
             * Schedule next transfer.
             *
             * Do NOT start it directly from the
             * event handler.
             */
            s_launch_pending = true;

            break;


        default:
            break;
    }
}


/*=============================================================================
 * Status Access
 *============================================================================*/

bool i2c_interrupt_hil_is_done(void)
{
    return ((s_report.state == I2C_HIL_DONE) ||
            (s_report.state == I2C_HIL_FAILED));
}


const i2c_hil_report_t *
i2c_interrupt_hil_get_report(void)
{
    return &s_report;
}