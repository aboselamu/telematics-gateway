#ifndef CLOCK_F103_H
#define CLOCK_F103_H

#include <stdint.h>

typedef enum
{
    CLOCK_F103_OK = 0,
    CLOCK_F103_ERR_HSE_TIMEOUT = 1,
    CLOCK_F103_ERR_PLL_TIMEOUT = 2,
    CLOCK_F103_ERR_SWITCH_TIMEOUT = 3
} clock_f103_status_t;

/* Requires an actual 8 MHz external crystal (HSE) on the STM32F103 board. */
clock_f103_status_t ClockF103_Init72MHz(void);

#endif /* CLOCK_F103_H */
