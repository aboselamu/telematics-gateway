#include "stm32f4xx.h"
#include "stm32f446xx.h"

#include "i2c_driver.h"
#include "i2c_driver_it.h"

#include "event_queue.h"
#include "i2c_interrupt_hil.h"


/*=============================================================================
 * Debug / HIL Observation
 *============================================================================*/

/*
 * These mirrors make the final HIL result easy to inspect
 * from the debugger without exposing test internals.
 */
volatile uint32_t g_i2c_hil_done = 0U;
volatile uint32_t g_i2c_hil_tests_passed = 0U;
volatile uint32_t g_i2c_hil_tests_failed = 0U;
volatile uint32_t g_i2c_hil_stress_completed = 0U;
volatile uint32_t g_i2c_hil_event_post_failures = 0U;

volatile i2c_status_t g_i2c_hil_last_result = I2C_OK;
volatile i2c_hil_state_t g_i2c_hil_final_state = I2C_HIL_IDLE;

volatile i2c_status_t g_i2c_init_status;
volatile event_status_t g_event_queue_init_status;


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
 * Main
 *============================================================================*/

int main(void)
{
    const i2c_config_t i2c_config =
    {
        .clock_speed = I2C_CLOCK_STANDARD_SAFE_HZ
    };

    event_t event;

    const i2c_hil_report_t *p_report;


    /*----------------------------------------------------------
     * 1. MCU clock
     *---------------------------------------------------------*/

    SystemClock_Config();


    /*----------------------------------------------------------
     * 2. Physical I2C peripheral
     *
     * Existing, already HIL-verified configuration.
     *---------------------------------------------------------*/

    g_i2c_init_status =
        i2c_init(&i2c_config);

    if (g_i2c_init_status != I2C_OK)
    {
        while (1)
        {
            __NOP();
        }
    }


    /*----------------------------------------------------------
     * 3. Middleware
     *---------------------------------------------------------*/

    g_event_queue_init_status =
        eventQueue_init();

    if (g_event_queue_init_status != EVENT_QUEUE_OK)
    {
        while (1)
        {
            __NOP();
        }
    }


    /*----------------------------------------------------------
     * 4. Interrupt I2C HIL
     *
     * Internally:
     *
     * i2c_interrupt_hil_init()
     *      -> i2c_it_init()
     *      -> registers I2C completion callback
     *---------------------------------------------------------*/

    i2c_interrupt_hil_init();

    i2c_interrupt_hil_start();


    /*----------------------------------------------------------
     * 5. Cooperative super-loop
     *---------------------------------------------------------*/

    while (1)
    {
        /*
         * Launch pending I2C HIL transactions.
         *
         * This never blocks waiting for the transfer.
         */
        i2c_interrupt_hil_process();


        /*
         * Drain middleware events.
         */
        while (eventQueue_poll(&event) ==
               EVENT_QUEUE_OK)
        {
            switch (event.event_id)
            {
                case EVT_I2C_TRANSFER_DONE:

                    i2c_interrupt_hil_handle_event(
                        &event);

                    break;


                case EVT_UART_FRAME_READY:

                    /*
                     * Existing UART/frame path will
                     * eventually live here again.
                     */
                    break;


                case EVT_NONE:
                default:

                    break;
            }
        }


        /*
         * Mirror HIL result for debugger inspection.
         */
        p_report =
            i2c_interrupt_hil_get_report();

        g_i2c_hil_tests_passed =
            p_report->tests_passed;

        g_i2c_hil_tests_failed =
            p_report->tests_failed;

        g_i2c_hil_stress_completed =
            p_report->stress_completed;

        g_i2c_hil_event_post_failures =
            p_report->event_post_failures;

        g_i2c_hil_last_result =
            p_report->last_result;

        g_i2c_hil_final_state =
            p_report->state;


        if (i2c_interrupt_hil_is_done())
        {
            g_i2c_hil_done = 1U;

            /*
             * Keep firmware alive so debugger values
             * remain observable.
             */
            while (1)
            {
                __NOP();
            }
        }
    }
}