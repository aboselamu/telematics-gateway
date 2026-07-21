typedef enum
{
    I2C_OK = 0,

    I2C_ERR_PARAM,
    I2C_ERR_BUSY,
    I2C_ERR_TIMEOUT,
    I2C_ERR_NACK,
    I2C_ERR_BUS

} i2c_status_t;

typedef enum
{
    I2C_STATE_RESET = 0,

    I2C_STATE_READY,

    I2C_STATE_BUSY_TX,

    I2C_STATE_BUSY_RX,

    I2C_STATE_ERROR

} i2c_state_t;

typedef struct
{
    uint32_t clock_speed;

} i2c_config_t;

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