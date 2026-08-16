#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "i2c_driver_it.h"
#include "event_queue.h"


typedef enum
{
    I2C_HIL_IDLE = 0,

    I2C_HIL_READ_1_BYTE,
    I2C_HIL_READ_2_BYTES,
    I2C_HIL_READ_7_BYTES,

    I2C_HIL_SEPARATE_WRITE,
    I2C_HIL_SEPARATE_READ,

    I2C_HIL_PARAMETER_TEST,
    I2C_HIL_BUSY_TEST,
    I2C_HIL_NACK_TEST,

    I2C_HIL_STRESS_TEST,

    I2C_HIL_DONE,
    I2C_HIL_FAILED

} i2c_hil_state_t;


typedef struct
{
    uint32_t tests_passed;
    uint32_t tests_failed;

    uint32_t stress_completed;
    uint32_t event_post_failures;

    i2c_status_t last_result;
    i2c_hil_state_t state;

} i2c_hil_report_t;


void i2c_interrupt_hil_init(void);

void i2c_interrupt_hil_start(void);

void i2c_interrupt_hil_process(void);

void i2c_interrupt_hil_handle_event(
    const event_t *p_event);

bool i2c_interrupt_hil_is_done(void);

const i2c_hil_report_t *
i2c_interrupt_hil_get_report(void);