#include <stdio.h>
// #include "board.h"        
#include "stm32f4xx.h"
#include "stm32f446xx.h" // Ensure your device header is included
#include <stdint.h>
#include "i2c_driver.h"

/* DS3231 uses a 7-bit I2C address. */
#define DS3231_I2C_ADDRESS       0x68U

/* Register 0x00 is the seconds register. */
#define DS3231_SECONDS_REGISTER  0x00U

/*
 * Debug variables.
 *
 * Declared volatile so they remain visible and are not removed
 * by compiler optimisation.
 */
volatile i2c_status_t g_i2c_init_status;
volatile i2c_status_t g_i2c_transfer_status;

volatile i2c_state_t g_i2c_state_after_init;
volatile i2c_state_t g_i2c_state_after_transfer;

volatile i2c_status_t g_write_status;
volatile i2c_status_t g_read_status;

volatile i2c_state_t g_state_after_write;
volatile i2c_state_t g_state_after_read;

volatile uint8_t g_separate_read_data;

volatile uint8_t g_rtc_seconds_raw;
volatile uint8_t g_rtc_minutes_raw;
volatile uint8_t g_rtc_raw[7];
volatile uint8_t g_rtc_two_raw[2];

//recovery
volatile uint32_t g_recovery_test_stage = 0U;

volatile i2c_status_t g_fault_transfer_status =
    (i2c_status_t)0xFF;

volatile i2c_state_t g_state_after_fault =
    I2C_STATE_RESET;

volatile i2c_status_t g_recover_while_stuck_status =
    (i2c_status_t)0xFF;

volatile i2c_state_t g_state_after_failed_recover =
    I2C_STATE_RESET;

volatile i2c_status_t g_recover_after_release_status =
    (i2c_status_t)0xFF;

volatile i2c_state_t g_state_after_successful_recover =
    I2C_STATE_RESET;

volatile i2c_status_t g_post_recovery_transfer_status =
    (i2c_status_t)0xFF;

volatile i2c_state_t g_state_after_post_recovery_transfer =
    I2C_STATE_RESET;

volatile uint8_t g_post_recovery_data[2];
// variables

volatile uint32_t g_test_count = 0U;
volatile uint32_t g_ok_count = 0U;
volatile uint32_t g_error_count = 0U;
volatile i2c_status_t g_last_status;
volatile i2c_state_t g_final_state;
volatile i2c_status_t g_null_write_result;
volatile i2c_status_t g_null_read_result;
volatile i2c_status_t g_null_txrx_tx_result;
volatile i2c_status_t g_null_txrx_rx_result;
volatile i2c_state_t  g_state_after_parameter_tests;
/**
* @brief  SystemInit placeholder to satisfy startup assembly requirements
*/
// void SystemInit(void) {
//     /* Intentionally left blank for pure bare-metal execution */
// }

/*
 * Keep your previously tested SystemClock_Config() function here.
 *
 * Required clock configuration:
 *
 * SYSCLK = 180 MHz
 * APB1   = 45 MHz
 * APB2   = 90 MHz
 */
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

    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL)
    {
    }

    // SystemCoreClockUpdate(); // in future 
}

int main(void)
{
    const i2c_config_t i2c_config =
    {
        .clock_speed = I2C_CLOCK_STANDARD_SAFE_HZ
    };

    uint8_t register_address = DS3231_SECONDS_REGISTER;
    uint8_t rtc_data[2] = {0U};
    // uint8_t register_address = DS3231_SECONDS_REGISTER;
    // uint8_t rtc_data[2] = {0U, 0U};

    SystemClock_Config();

    g_i2c_init_status = i2c_init(&i2c_config);
    g_i2c_state_after_init = i2c_get_state();

    if (g_i2c_init_status != I2C_OK)
    {
        while (1)
        {
            __NOP();
        }
    }


    g_null_write_result = i2c_write(DS3231_I2C_ADDRESS, NULL, 1U);

    g_null_read_result =
        i2c_read(DS3231_I2C_ADDRESS, NULL, 1U);

    g_null_txrx_tx_result =
        i2c_write_read(
            DS3231_I2C_ADDRESS,
            NULL,
            1U,
            rtc_data,
            2U);

    g_null_txrx_rx_result =
        i2c_write_read(
            DS3231_I2C_ADDRESS,
            &register_address,
            1U,
            NULL,
            2U);

    g_state_after_parameter_tests = i2c_get_state();

    while (1)
    {
        __NOP();
    }
}
