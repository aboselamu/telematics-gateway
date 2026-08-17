#ifndef DS3231_H
#define DS3231_H

#include <stdint.h>
#include "i2c_driver.h"


typedef enum
{
    DS3231_OK = 0,
    DS3231_BUSY,
    DS3231_ERR_PARAM,
    DS3231_ERR_TRANSPORT,
    DS3231_ERR_DATA

} ds3231_status_t;


typedef enum
{
    DS3231_STATE_IDLE = 0,
    DS3231_STATE_READING_TIME,
    DS3231_STATE_WRITING_TIME,
    DS3231_STATE_ERROR

} ds3231_state_t;


typedef struct
{
    uint8_t seconds;
    uint8_t minutes;
    uint8_t hours;

    uint8_t day;
    uint8_t date;
    uint8_t month;

    uint16_t year;

} ds3231_time_t;


/*
 * Initialise software state.
 *
 * Physical I2C peripheral must already be initialised.
 */
void ds3231_init(void);


/*
 * Start asynchronous time/date acquisition.
 *
 * Returns immediately.
 */
ds3231_status_t ds3231_read_time_async(void);


/*
 * Start asynchronous time/date write.
 */
ds3231_status_t ds3231_set_time_async(
    const ds3231_time_t *p_time);


/*
 * Obtain the most recently completed valid reading.
 */
ds3231_status_t ds3231_get_time(
    ds3231_time_t *p_time);


ds3231_state_t ds3231_get_state(void);


/*
 * Called when the underlying I2C transaction completes.
 *
 * Later we may make this private when I2C supports
 * per-transaction callbacks.
 */
void ds3231_on_i2c_complete(
    i2c_status_t result);


#endif