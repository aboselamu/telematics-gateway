#include "ds3231.h"
#include "i2c_driver_it.h"

#include <stddef.h>


/*=============================================================================
 * DS3231 Device Definition
 *============================================================================*/

#define DS3231_I2C_ADDRESS          0x68U

#define DS3231_REG_SECONDS          0x00U
#define DS3231_REG_MINUTES          0x01U
#define DS3231_REG_HOURS            0x02U
#define DS3231_REG_DAY              0x03U
#define DS3231_REG_DATE             0x04U
#define DS3231_REG_MONTH            0x05U
#define DS3231_REG_YEAR             0x06U

#define DS3231_REG_CONTROL          0x0EU
#define DS3231_REG_STATUS           0x0FU

#define DS3231_REG_TEMP_MSB         0x11U
#define DS3231_REG_TEMP_LSB         0x12U

#define DS3231_TIME_REGISTER_COUNT  7U


/*=============================================================================
 * Private Context
 *============================================================================*/

typedef struct
{
    ds3231_state_t state;

    ds3231_status_t result;

    ds3231_time_t time;

    uint8_t register_address;

    uint8_t rx_buffer[DS3231_TIME_REGISTER_COUNT];

    uint8_t tx_buffer[8];

    uint8_t data_valid;

} ds3231_context_t;


static ds3231_context_t s_ds3231;


/*=============================================================================
 * Private Conversion Functions
 *============================================================================*/

static uint8_t ds3231_bcd_to_binary(
    uint8_t value)
{
    return (uint8_t)(
        ((value >> 4U) * 10U) +
        (value & 0x0FU));
}


static uint8_t ds3231_binary_to_bcd(
    uint8_t value)
{
    return (uint8_t)(
        ((value / 10U) << 4U) |
        (value % 10U));
}


/*=============================================================================
 * Private Validation
 *============================================================================*/

static uint8_t ds3231_time_is_valid(
    const ds3231_time_t *p_time)
{
    if (p_time == NULL)
    {
        return 0U;
    }

    if (p_time->seconds > 59U)
    {
        return 0U;
    }

    if (p_time->minutes > 59U)
    {
        return 0U;
    }

    if (p_time->hours > 23U)
    {
        return 0U;
    }

    if ((p_time->day < 1U) ||
        (p_time->day > 7U))
    {
        return 0U;
    }

    if ((p_time->date < 1U) ||
        (p_time->date > 31U))
    {
        return 0U;
    }

    if ((p_time->month < 1U) ||
        (p_time->month > 12U))
    {
        return 0U;
    }

    if ((p_time->year < 2000U) ||
        (p_time->year > 2199U))
    {
        return 0U;
    }

    return 1U;
}


// helper
static ds3231_status_t ds3231_map_i2c_status(
    i2c_status_t status)
{
    if (status == I2C_OK)
    {
        return DS3231_OK;
    }

    /*
     * These mean the operation was not accepted yet.
     * The caller may retry later.
     */
    if ((status == I2C_ERR_BUSY) ||
        (status == I2C_ERR_BUS))
    {
        return DS3231_BUSY;
    }

    return DS3231_ERR_TRANSPORT;
}

/*=============================================================================
 * Decode Raw Register Data
 *============================================================================*/

static ds3231_status_t ds3231_decode_time(void)
{
    ds3231_time_t time;

    uint8_t hour_register;
    uint8_t month_register;


    time.seconds =
        ds3231_bcd_to_binary(
            s_ds3231.rx_buffer[0] & 0x7FU);

    time.minutes =
        ds3231_bcd_to_binary(
            s_ds3231.rx_buffer[1] & 0x7FU);


    /*----------------------------------------------------------
     * Hours
     *
     * DS3231 can operate in 12-hour or 24-hour mode.
     *---------------------------------------------------------*/

    hour_register =
        s_ds3231.rx_buffer[2];

    if ((hour_register & 0x40U) == 0U)
    {
        /* 24-hour mode */

        time.hours =
            ds3231_bcd_to_binary(
                hour_register & 0x3FU);
    }
    else
    {
        uint8_t hour_12;
        uint8_t is_pm;

        hour_12 =
            ds3231_bcd_to_binary(
                hour_register & 0x1FU);

        is_pm =
            (hour_register & 0x20U) != 0U;

        if (hour_12 == 12U)
        {
            time.hours =
                is_pm ? 12U : 0U;
        }
        else
        {
            time.hours =
                is_pm ?
                    (uint8_t)(hour_12 + 12U) :
                    hour_12;
        }
    }


    time.day =
        ds3231_bcd_to_binary(
            s_ds3231.rx_buffer[3] & 0x07U);

    time.date =
        ds3231_bcd_to_binary(
            s_ds3231.rx_buffer[4] & 0x3FU);


    month_register =
        s_ds3231.rx_buffer[5];

    time.month =
        ds3231_bcd_to_binary(
            month_register & 0x1FU);


    time.year =
        (uint16_t)(
            2000U +
            ds3231_bcd_to_binary(
                s_ds3231.rx_buffer[6]));

    /*
     * DS3231 century bit.
     */
    if ((month_register & 0x80U) != 0U)
    {
        time.year += 100U;
    }


    if (!ds3231_time_is_valid(&time))
    {
        return DS3231_ERR_DATA;
    }


    s_ds3231.time = time;

    return DS3231_OK;
}


/*=============================================================================
 * Public Initialisation
 *============================================================================*/

void ds3231_init(void)
{
    s_ds3231.state =
        DS3231_STATE_IDLE;

    s_ds3231.result =
        DS3231_OK;

    s_ds3231.data_valid = 0U;

    s_ds3231.register_address =
        DS3231_REG_SECONDS;
}


/*=============================================================================
 * Read Time
 *============================================================================*/

ds3231_status_t ds3231_read_time_async(void)
{
    i2c_status_t status;


    if (s_ds3231.state != DS3231_STATE_IDLE)
    {
        return DS3231_BUSY;
    }


    s_ds3231.register_address =
        DS3231_REG_SECONDS;


    status =
        i2c_write_read_it(
            DS3231_I2C_ADDRESS,
            &s_ds3231.register_address,
            1U,
            s_ds3231.rx_buffer,
            DS3231_TIME_REGISTER_COUNT);


    if (status != I2C_OK)
    {
        ds3231_status_t device_status =
            ds3231_map_i2c_status(status);

        /*
        * Transaction was not started,
        * therefore release device ownership.
        */
        s_ds3231.state =
            DS3231_STATE_IDLE;

        s_ds3231.result =
            device_status;

        return device_status;
    }


    s_ds3231.state =
        DS3231_STATE_READING_TIME;

    return DS3231_OK;
}


/*=============================================================================
 * Write Time
 *============================================================================*/

ds3231_status_t ds3231_set_time_async(
    const ds3231_time_t *p_time)
{
    i2c_status_t status;

    uint16_t year;


    if (!ds3231_time_is_valid(p_time))
    {
        return DS3231_ERR_PARAM;
    }


    if (s_ds3231.state != DS3231_STATE_IDLE)
    {
        return DS3231_BUSY;
    }


    /*
     * First byte is register address.
     */
    s_ds3231.tx_buffer[0] =
        DS3231_REG_SECONDS;


    s_ds3231.tx_buffer[1] =
        ds3231_binary_to_bcd(
            p_time->seconds);

    s_ds3231.tx_buffer[2] =
        ds3231_binary_to_bcd(
            p_time->minutes);

    /*
     * Explicitly write 24-hour representation.
     */
    s_ds3231.tx_buffer[3] =
        ds3231_binary_to_bcd(
            p_time->hours);

    s_ds3231.tx_buffer[4] =
        ds3231_binary_to_bcd(
            p_time->day);

    s_ds3231.tx_buffer[5] =
        ds3231_binary_to_bcd(
            p_time->date);


    year = p_time->year;


    if (year >= 2100U)
    {
        s_ds3231.tx_buffer[6] =
            (uint8_t)(
                ds3231_binary_to_bcd(
                    p_time->month) |
                0x80U);

        year -= 2100U;
    }
    else
    {
        s_ds3231.tx_buffer[6] =
            ds3231_binary_to_bcd(
                p_time->month);

        year -= 2000U;
    }


    s_ds3231.tx_buffer[7] =
        ds3231_binary_to_bcd(
            (uint8_t)year);


    status =
        i2c_write_it(
            DS3231_I2C_ADDRESS,
            s_ds3231.tx_buffer,
            sizeof(s_ds3231.tx_buffer));


    if (status != I2C_OK)
    {
        ds3231_status_t device_status =
            ds3231_map_i2c_status(status);

        /*
        * Transaction was not started,
        * therefore release device ownership.
        */
        s_ds3231.state =
            DS3231_STATE_IDLE;

        s_ds3231.result =
            device_status;

        return device_status;
    }


    s_ds3231.state =
        DS3231_STATE_WRITING_TIME;

    return DS3231_OK;
}


/*=============================================================================
 * I2C Completion
 *============================================================================*/

void ds3231_on_i2c_complete(
    i2c_status_t result)
{
    ds3231_status_t decode_result;


    if ((s_ds3231.state != DS3231_STATE_READING_TIME) &&
        (s_ds3231.state != DS3231_STATE_WRITING_TIME))
    {
        return;
    }


    if (result != I2C_OK)
    {
        s_ds3231.result =
            DS3231_ERR_TRANSPORT;

        s_ds3231.state =
            DS3231_STATE_ERROR;

        return;
    }


    if (s_ds3231.state ==
        DS3231_STATE_READING_TIME)
    {
        decode_result =
            ds3231_decode_time();

        if (decode_result != DS3231_OK)
        {
            s_ds3231.result =
                decode_result;

            s_ds3231.state =
                DS3231_STATE_ERROR;

            return;
        }


        s_ds3231.data_valid = 1U;
    }


    s_ds3231.result =
        DS3231_OK;

    s_ds3231.state =
        DS3231_STATE_IDLE;
}


/*=============================================================================
 * Data Access
 *============================================================================*/

ds3231_status_t ds3231_get_time(
    ds3231_time_t *p_time)
{
    if (p_time == NULL)
    {
        return DS3231_ERR_PARAM;
    }


    if (s_ds3231.data_valid == 0U)
    {
        return DS3231_ERR_DATA;
    }


    *p_time =
        s_ds3231.time;

    return DS3231_OK;
}


ds3231_state_t ds3231_get_state(void)
{
    return s_ds3231.state;
}


/*
===============================================================================
DS3231 Driver Summary & Assumptions
===============================================================================

Overview:
- Implements an asynchronous I²C driver for DS3231 RTC (address 0x68).
- Uses a private context (s_ds3231) with state machine, buffers, and last valid time.
- Provides conversion helpers, validation, async read/write, and error handling.

Key Functions:
- ds3231_bcd_to_binary() / ds3231_binary_to_bcd()
    -> Convert between DS3231 BCD register format and binary values.

- ds3231_time_is_valid()
    -> Validate time/date fields (seconds, minutes, hours, day, date, month, year).

- ds3231_decode_time()
    -> Decode raw register data into ds3231_time_t struct.
    -> Handles 12-hour vs 24-hour mode and century bit.

- ds3231_init()
    -> Initialize driver context (state = IDLE, result = OK, data_valid = 0).

- ds3231_read_time_async()
    -> Start asynchronous I²C read of time registers.
    -> Sets state to READING_TIME.

- ds3231_set_time_async()
    -> Validate input time, convert to BCD, write registers asynchronously.
    -> Sets state to WRITING_TIME.

- ds3231_on_i2c_complete()
    -> I²C completion callback.
    -> If reading: decode time and validate.
    -> If writing: mark success.
    -> Updates state and result accordingly.

- ds3231_get_time()
    -> Return last valid time if available, else error.

- ds3231_get_state()
    -> Return current driver state (IDLE, READING, WRITING, ERROR).

Assumptions:
- Relies on interrupt-driven I²C layer (i2c_driver_it.h).
- Application must call ds3231_init() before use.
- Time written is expected in 24-hour format.
- Century bit used for years >= 2100.
- Validation ensures only correct time values are accepted.
- Asynchronous model requires waiting for completion before accessing data.

===============================================================================
*/

