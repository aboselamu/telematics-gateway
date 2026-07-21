/**
 * @file frame_manager.c
 * @brief Frame pool manager.
 */

#include "frame_manager.h"
#include "event_queue.h"

static frame_t s_frame_pool[FRAME_COUNT];

void frame_manager_process_byte(uint8_t byte)
{
    static int8_t current_frame = -1;
        /* Ignore carriage return */
    if (byte == '\r')
    {
        return;
    }

    if (current_frame < 0)
    {
        current_frame = frame_manager_allocate();

        if (current_frame < 0)
        {
            return;     // No free frame available
        }
    }

    if (!frame_manager_write_byte(current_frame, byte)){
        frame_manager_release(current_frame);
        current_frame = -1;
        return;
    }

    if (byte == '\n')
    {
        frame_t *frame = frame_manager_get(current_frame);

        /* Replace '\n' with string terminator */
        if (frame->length > 0)
        {
            frame->data[frame->length - 1] = '\0';
        }

        frame_manager_commit(current_frame);

        event_t evt = {0};
        evt.event_id = EVT_UART_FRAME_READY;
        evt.param1   = (uint32_t)frame;
        evt.param2   = current_frame;

        eventQueue_post(&evt);

        current_frame = -1;
    }
}


void frame_manager_init(void)
{
    for (uint8_t i = 0; i < FRAME_COUNT; i++)
    {
        s_frame_pool[i].length = 0;
        s_frame_pool[i].in_use = false;
    }
}

int8_t frame_manager_allocate(void)
{
    for (uint8_t i = 0; i < FRAME_COUNT; i++)
    {
        if (!s_frame_pool[i].in_use)
        {
            s_frame_pool[i].in_use = true;
            s_frame_pool[i].length = 0;
            return i;
        }
    }

    return -1;
}

bool frame_manager_write_byte(int8_t frame_id, uint8_t byte)
{
    if ((frame_id < 0) || (frame_id >= FRAME_COUNT))
    {
        return false;
    }

    if (s_frame_pool[frame_id].length >= FRAME_MAX_SIZE)
    {
        return false;
    }

    s_frame_pool[frame_id].data[s_frame_pool[frame_id].length++] = byte;

    return true;
}

void frame_manager_commit(int8_t frame_id)
{
    (void)frame_id;
    /* Nothing to do yet.
       Later we can add CRC, timestamp,
       ownership flags, etc. */
}

frame_t *frame_manager_get(int8_t frame_id)
{
    if ((frame_id < 0) || (frame_id >= FRAME_COUNT))
    {
        return 0;
    }

    return &s_frame_pool[frame_id];
}

void frame_manager_release(int8_t frame_id)
{
    if ((frame_id < 0) || (frame_id >= FRAME_COUNT))
    {
        return;
    }

    s_frame_pool[frame_id].length = 0;
    s_frame_pool[frame_id].in_use = false;
}