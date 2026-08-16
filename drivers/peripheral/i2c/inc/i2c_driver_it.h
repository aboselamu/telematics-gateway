#ifndef I2C_DRIVER_IT_H
#define I2C_DRIVER_IT_H

#include <stdint.h>
#include "i2c_driver.h"

/*
 * Interrupt-driven I2C driver.
 *
 * Transaction APIs are non-blocking.
 *
 * I2C_OK returned from i2c_write_it(), i2c_read_it(),
 * or i2c_write_read_it() means that the transaction
 * was accepted and started.
 *
 * Final transaction status must be obtained separately
 * after the driver leaves a BUSY state.
 */

/* Interrupt engine initialisation */
void i2c_it_init(void);

/* Non-blocking write */
i2c_status_t i2c_write_it(
    uint8_t address,
    const uint8_t *p_data,
    uint16_t length);

/* Non-blocking read */
i2c_status_t i2c_read_it(
    uint8_t address,
    uint8_t *p_data,
    uint16_t length);

/* Non-blocking combined write/repeated-start/read */
i2c_status_t i2c_write_read_it(
    uint8_t address,
    const uint8_t *p_tx_data,
    uint16_t tx_length,
    uint8_t *p_rx_data,
    uint16_t rx_length);

/* Transaction observation */
i2c_state_t i2c_it_get_state(void);
i2c_status_t i2c_it_get_result(void);

typedef void (*i2c_it_callback_t)(i2c_status_t result);
void i2c_it_register_callback(i2c_it_callback_t callback);

#endif /* I2C_DRIVER_IT_H */