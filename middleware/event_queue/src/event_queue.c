#include "event_queue.h"

#include "stm32f4xx.h" 

static event_t  s_queue[EVENT_QUEUE_DEPTH]; 
static uint16_t s_head = 0;
static uint16_t s_tail = 0;

event_status_t eventQueue_init(void) {
    s_head = 0;
    s_tail = 0;
    return EVENT_QUEUE_OK;
}

event_status_t eventQueue_post(const event_t *p_event) {
    ASSERT(p_event != NULL); // Use the hardened macro
    
    uint32_t primask_state = __get_PRIMASK();
    __disable_irq();

    uint16_t next_head = (s_head + 1) & (EVENT_QUEUE_DEPTH - 1);
    if (next_head == s_tail) {
        __set_PRIMASK(primask_state);
        return EVENT_QUEUE_FULL;
    }
    
    s_queue[s_head] = *p_event;
    s_head = next_head;
    
    __set_PRIMASK(primask_state);
    return EVENT_QUEUE_OK;
}

event_status_t eventQueue_poll(event_t *p_event) {
    ASSERT(p_event != NULL);
    
    uint32_t primask_state = __get_PRIMASK();
    __disable_irq();

    if (s_head == s_tail) {
        __set_PRIMASK(primask_state);
        return EVENT_QUEUE_EMPTY;
    }
    
    *p_event = s_queue[s_tail];
    s_tail = (s_tail + 1) & (EVENT_QUEUE_DEPTH - 1);
    
    __set_PRIMASK(primask_state);
    return EVENT_QUEUE_OK;
}