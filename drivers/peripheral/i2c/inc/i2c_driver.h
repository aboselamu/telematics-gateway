#ifndef I2C_DRIVER_H
#define I2C_DRIVER_H

#include <stdint.h>

#define I2C_CLOCK_STANDARD_SAFE_HZ   88000UL // for testing, refer STM32F446 erratum


typedef enum {
    I2C_OK             = 0x00,
    I2C_ERR_BUSY       = 0x01,  //  Another driver transaction is active. 
    I2C_ERR_LOCKED     = 0x02,  // Driver caught an error, call i2c_recover()
    I2C_ERR_NOT_INIT   = 0x03,  // Driver is still in RESET/Uninitialized state 
    I2C_ERR_TIMEOUT    = 0x04,
    I2C_ERR_NACK       = 0x05,
    I2C_ERR_PARAM              = 0x06,
    I2C_ERR_BUS                = 0x07,
    I2C_ERR_ARBITRATION_LOST   = 0x08,
    I2C_ERR_OVERRUN            = 0x09
    // I2C_ERR_BUS_BUSY = 0x0AU        //physical I2C bus is not idle
    // I2C_ERR_ARBITRATION_LOST
    // I2C_ERR_OVERRUN
} i2c_status_t;


typedef enum
{
    I2C_STATE_RESET = 0,

    I2C_STATE_READY,

    I2C_STATE_BUSY_TX,

    I2C_STATE_BUSY_RX,

    I2C_STATE_BUSY_TX_RX,

    I2C_STATE_ERROR

} i2c_state_t;

typedef struct
{
    uint32_t clock_speed;

} i2c_config_t;

//Public API
i2c_status_t i2c_init(const i2c_config_t *p_config);

// to write, the const indicates that the function doesn't change the data
i2c_status_t i2c_write(
    uint8_t device_address,
    const uint8_t *p_data,
    uint16_t length);

// to read
i2c_status_t i2c_read(
    uint8_t device_address,
    uint8_t *p_data,
    uint16_t length);

// write and read    
i2c_status_t i2c_write_read(uint8_t device_address, const uint8_t *p_tx_data,
                             uint16_t tx_length, uint8_t *p_rx_data, uint16_t rx_length);

i2c_state_t i2c_get_state(void);
i2c_status_t i2c_recover(void);
#endif