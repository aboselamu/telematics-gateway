#include "stm32f4xx.h"
#include "stm32f446xx.h"

#include "i2c_driver.h"
#include "i2c_driver_it.h"

#include "event_queue.h"
#include "ds3231.h"


/*=============================================================================
 * Debug / HIL Observation
 *============================================================================*/

volatile i2c_status_t g_i2c_init_status;
volatile event_status_t g_event_queue_init_status;


/*=============================================================================
 * DS3231 HIL Test State
 *============================================================================*/

typedef enum
{
    DS3231_TEST_WRITE = 0,
    DS3231_TEST_WAIT_WRITE,
    DS3231_TEST_READ,
    DS3231_TEST_WAIT_READ,
    DS3231_TEST_DONE,
    DS3231_TEST_FAILED

} ds3231_test_state_t;


volatile ds3231_test_state_t g_ds3231_test_state =
    DS3231_TEST_WRITE;

volatile ds3231_status_t g_ds3231_write_status =
    DS3231_OK;

volatile ds3231_status_t g_ds3231_read_status =
    DS3231_OK;

volatile ds3231_status_t g_ds3231_get_status =
    DS3231_ERR_DATA;

volatile uint32_t g_ds3231_test_done = 0U;
volatile uint32_t g_ds3231_test_pass = 0U;

volatile uint32_t g_i2c_event_post_failures = 0U;

volatile ds3231_time_t g_ds3231_readback;


/*
 * Known value written to the RTC and then read back.
 *
 * Day numbering is application-defined by the DS3231.
 * Here:
 *      1 = Sunday
 */
static const ds3231_time_t s_test_time =
{
    .seconds = 0U,
    .minutes = 55U,
    .hours   = 12U,

    .day     = 1U,
    .date    = 16U,
    .month   = 8U,
    .year    = 2026U
};


/*=============================================================================
 * System Clock
 *============================================================================*/

void SystemClock_Config(void)
{
    RCC->CR |= RCC_CR_HSION;

    while ((RCC->CR & RCC_CR_HSIRDY) == 0U)
    {
    }

    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    PWR->CR |= PWR_CR_VOS;

    FLASH->ACR =
        FLASH_ACR_ICEN |
        FLASH_ACR_DCEN |
        FLASH_ACR_LATENCY_5WS;

    RCC->PLLCFGR =
        (16U  << RCC_PLLCFGR_PLLM_Pos) |
        (360U << RCC_PLLCFGR_PLLN_Pos) |
        (0U   << RCC_PLLCFGR_PLLP_Pos) |
        RCC_PLLCFGR_PLLSRC_HSI;

    RCC->CR |= RCC_CR_PLLON;

    while ((RCC->CR & RCC_CR_PLLRDY) == 0U)
    {
    }

    RCC->CFGR |=
        RCC_CFGR_HPRE_DIV1 |
        RCC_CFGR_PPRE1_DIV4 |
        RCC_CFGR_PPRE2_DIV2;

    RCC->CFGR |= RCC_CFGR_SW_PLL;

    while ((RCC->CFGR & RCC_CFGR_SWS) !=
           RCC_CFGR_SWS_PLL)
    {
    }
}


/*=============================================================================
 * I2C Completion Adapter
 *
 * Executes in I2C interrupt context.
 *
 * Keep this function very small. It only converts the driver callback into
 * a middleware event.
 *============================================================================*/

static void i2c_completion_callback(
    i2c_status_t result)
{
    event_t event = {0};

    event.event_id =
        EVT_I2C_TRANSFER_DONE;

    event.param1 =
        (uint32_t)result;

    if (eventQueue_post(&event) !=
        EVENT_QUEUE_OK)
    {
        g_i2c_event_post_failures++;
    }
}


/*=============================================================================
 * Main
 *============================================================================*/

int main(void)
{
    const i2c_config_t i2c_config =
    {
        .clock_speed =
            I2C_CLOCK_STANDARD_SAFE_HZ
    };

    event_t event;


    /*=========================================================================
     * 1. MCU Clock
     *========================================================================*/

    SystemClock_Config();


    /*=========================================================================
     * 2. I2C Peripheral Layer
     *========================================================================*/

    g_i2c_init_status =
        i2c_init(&i2c_config);

    if (g_i2c_init_status != I2C_OK)
    {
        while (1)
        {
            __NOP();
        }
    }


    /*=========================================================================
     * 3. Middleware
     *========================================================================*/

    g_event_queue_init_status =
        eventQueue_init();

    if (g_event_queue_init_status !=
        EVENT_QUEUE_OK)
    {
        while (1)
        {
            __NOP();
        }
    }


    /*=========================================================================
     * 4. Interrupt I2C Transport
     *========================================================================*/

    i2c_it_init();

    i2c_it_register_callback(
        i2c_completion_callback);


    /*=========================================================================
     * 5. DS3231 Device Layer
     *========================================================================*/

    ds3231_init();


    /*=========================================================================
     * 6. HIL Test - Start WRITE
     *
     * Write a known date/time first. The READ is started only after the
     * asynchronous WRITE completion event has been processed.
     *========================================================================*/

    g_ds3231_write_status =
        ds3231_set_time_async(&s_test_time);

    if (g_ds3231_write_status == DS3231_OK)
    {
        g_ds3231_test_state =
            DS3231_TEST_WAIT_WRITE;
    }
    else
    {
        g_ds3231_test_state =
            DS3231_TEST_FAILED;

        g_ds3231_test_pass = 0U;
        g_ds3231_test_done = 1U;
    }


    /*=========================================================================
     * Cooperative Super-loop
     *========================================================================*/

    while (1)
    {
        /*---------------------------------------------------------------------
         * Detect an event-queue failure.
         *
         * A lost completion event would otherwise leave the device-driver
         * state machine waiting indefinitely.
         *--------------------------------------------------------------------*/

        if ((g_i2c_event_post_failures != 0U) &&
            (g_ds3231_test_done == 0U))
        {
            g_ds3231_test_state =
                DS3231_TEST_FAILED;

            g_ds3231_test_pass = 0U;
            g_ds3231_test_done = 1U;
        }


        /*---------------------------------------------------------------------
         * Launch READ after WRITE completion.
         *
         * The WRITE completion event only schedules this state.
         * The next transaction is started here in normal thread context.
         *--------------------------------------------------------------------*/

        if (g_ds3231_test_state ==
            DS3231_TEST_READ)
        {
            g_ds3231_read_status =
                ds3231_read_time_async();

            if (g_ds3231_read_status ==
                DS3231_OK)
            {
                g_ds3231_test_state =
                    DS3231_TEST_WAIT_READ;
            }
            else if (g_ds3231_read_status ==
                     DS3231_BUSY)
            {
                /*
                 * The previous STOP may not yet have released the physical bus.
                 * Stay in DS3231_TEST_READ and retry on a later super-loop pass.
                 */
            }
            else
            {
                g_ds3231_test_state =
                    DS3231_TEST_FAILED;

                g_ds3231_test_pass = 0U;
                g_ds3231_test_done = 1U;
            }
        }


        /*---------------------------------------------------------------------
         * Middleware Event Dispatcher
         *--------------------------------------------------------------------*/

        while (eventQueue_poll(&event) ==
               EVENT_QUEUE_OK)
        {
            switch (event.event_id)
            {
                /*-------------------------------------------------------------
                 * I2C transaction completed.
                 *------------------------------------------------------------*/

                case EVT_I2C_TRANSFER_DONE:
                {
                    i2c_status_t result =
                        (i2c_status_t)
                            event.param1;


                    /*
                     * First allow the DS3231 device driver to process
                     * the completed transport transaction.
                     */
                    ds3231_on_i2c_complete(
                        result);


                    /*---------------------------------------------------------
                     * WRITE completion
                     *--------------------------------------------------------*/

                    if (g_ds3231_test_state ==
                        DS3231_TEST_WAIT_WRITE)
                    {
                        if ((result != I2C_OK) ||
                            (ds3231_get_state() !=
                             DS3231_STATE_IDLE))
                        {
                            g_ds3231_test_state =
                                DS3231_TEST_FAILED;

                            g_ds3231_test_pass = 0U;
                            g_ds3231_test_done = 1U;

                            break;
                        }


                        /*
                         * Do not start the READ directly here.
                         *
                         * Schedule it for the normal super-loop.
                         */
                        g_ds3231_test_state =
                            DS3231_TEST_READ;

                        break;
                    }


                    /*---------------------------------------------------------
                     * READ completion
                     *--------------------------------------------------------*/

                    if (g_ds3231_test_state ==
                        DS3231_TEST_WAIT_READ)
                    {
                        ds3231_time_t time;


                        if ((result != I2C_OK) ||
                            (ds3231_get_state() !=
                             DS3231_STATE_IDLE))
                        {
                            g_ds3231_test_state =
                                DS3231_TEST_FAILED;

                            g_ds3231_test_pass = 0U;
                            g_ds3231_test_done = 1U;

                            break;
                        }


                        /*
                         * Retrieve decoded application-level time.
                         */
                        g_ds3231_get_status =
                            ds3231_get_time(
                                &time);


                        if (g_ds3231_get_status !=
                            DS3231_OK)
                        {
                            g_ds3231_test_state =
                                DS3231_TEST_FAILED;

                            g_ds3231_test_pass = 0U;
                            g_ds3231_test_done = 1U;

                            break;
                        }


                        /*
                         * Save result so it is easy to inspect using GDB.
                         */
                        g_ds3231_readback =
                            time;


                        /*
                         * Verify write -> physical RTC -> read-back.
                         *
                         * Seconds are allowed to advance while the
                         * asynchronous transactions are executing.
                         */
                        if ((time.seconds <= 5U) &&
                            (time.minutes == 55U) &&
                            (time.hours   == 12U) &&
                            (time.day     == 1U) &&
                            (time.date    == 16U) &&
                            (time.month   == 8U) &&
                            (time.year    == 2026U))
                        {
                            g_ds3231_test_pass =
                                1U;

                            g_ds3231_test_state =
                                DS3231_TEST_DONE;
                        }
                        else
                        {
                            g_ds3231_test_pass =
                                0U;

                            g_ds3231_test_state =
                                DS3231_TEST_FAILED;
                        }


                        g_ds3231_test_done =
                            1U;

                        break;
                    }


                    break;
                }


                /*-------------------------------------------------------------
                 * Existing UART event path
                 *------------------------------------------------------------*/

                case EVT_UART_FRAME_READY:

                    /*
                     * Existing UART/frame processing.
                     */
                    break;


                case EVT_NONE:
                default:

                    break;
            }
        }


        /*
         * Leave the MCU running.
         *
         * The test result remains available through the global
         * debugger variables.
         */
    }
}