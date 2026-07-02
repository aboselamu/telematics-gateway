#pragma once
#include <stdint.h>
#include <stdbool.h>

#define EVENT_QUEUE_DEPTH 32

typedef enum {
    EVENT_QUEUE_OK,
    EVENT_QUEUE_FULL,
    EVT_UART_FRAME_READY,
    EVENT_QUEUE_EMPTY,
    EVENT_QUEUE_ERROR
} event_status_t;

typedef struct {
    uint8_t  event_id;
    uint32_t timestamp;
    uint32_t param1;
    uint32_t param2;
} event_t;

event_status_t eventQueue_init(void);
event_status_t eventQueue_post(const event_t *p_event);
event_status_t eventQueue_poll(event_t *p_event);