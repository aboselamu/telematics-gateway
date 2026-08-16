#pragma once

#include <stdint.h>

#define EVENT_QUEUE_DEPTH 32U
_Static_assert((EVENT_QUEUE_DEPTH & (EVENT_QUEUE_DEPTH - 1U)) == 0U, 
                          "EVENT_QUEUE_DEPTH must be a power of two");

typedef enum
{
    EVENT_QUEUE_OK = 0,
    EVENT_QUEUE_FULL,
    EVENT_QUEUE_EMPTY,
    EVENT_QUEUE_ERROR

} event_status_t;

typedef enum
{
    EVT_NONE = 0,

    EVT_UART_FRAME_READY,

    EVT_I2C_TRANSFER_DONE

} event_id_t;

typedef struct
{
    event_id_t event_id;

    uint32_t timestamp;
    uint32_t param1;
    uint32_t param2;

} event_t;

event_status_t eventQueue_init(void);
event_status_t eventQueue_post(const event_t *p_event);
event_status_t eventQueue_poll(event_t *p_event);