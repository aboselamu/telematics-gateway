#ifndef FRAME_MANAGER_H
#define FRAME_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

#define FRAME_COUNT      4
#define FRAME_MAX_SIZE   128

typedef struct
{
    uint8_t  data[FRAME_MAX_SIZE];
    uint16_t length;
    bool     in_use;
} frame_t;


/* Initialize the frame pool */
void frame_manager_init(void);

void frame_manager_process_byte(uint8_t byte);

/* Allocate an empty frame.
 * Returns frame index, or -1 if no free frame exists.
 */
int8_t frame_manager_allocate(void);

/* Append one byte to a frame */
bool frame_manager_write_byte(int8_t frame_id, uint8_t byte);

/* Mark frame as complete */
void frame_manager_commit(int8_t frame_id);

/* Get pointer to completed frame */
frame_t *frame_manager_get(int8_t frame_id);

/* Release frame back to the pool */
void frame_manager_release(int8_t frame_id);

#endif