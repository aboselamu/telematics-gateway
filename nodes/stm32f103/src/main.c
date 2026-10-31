
#include "stm32f1xx.h"
#include "clock_f103.h"
#include "can_f103.h"

/* Reverse-direction CAN HIL:
 * STM32F103 #1 -> STM32F446 gateway
 *
 * ID      = 0x321
 * DLC     = 1
 * DATA[0] = 0xA5
 * Bitrate = 500 kbit/s
 */

volatile uint32_t g_rev_f103_ready = 0U;
volatile uint32_t g_rev_f103_done = 0U;

volatile uint32_t g_rev_f103_clock_status = 0xFFFFFFFFUL;

volatile can_f103_status_t g_rev_f103_init_status =
    CAN_F103_ERR_INIT_TIMEOUT;

volatile can_f103_status_t g_rev_f103_tx_status =
    CAN_F103_ERR_TX_FAILED;

volatile uint32_t g_rev_f103_btr = 0U;
volatile uint32_t g_rev_f103_mcr = 0U;
volatile uint32_t g_rev_f103_esr = 0U;
volatile uint32_t g_rev_f103_tsr = 0U;

int main(void)
{
    /* 1. Configure SYSCLK = 72 MHz, PCLK1 = 36 MHz */
    g_rev_f103_clock_status =
        (uint32_t)ClockF103_Init72MHz();

    if (g_rev_f103_clock_status == CLOCK_F103_OK)
    {
        /* 2. CAN normal mode at 500 kbit/s */
        g_rev_f103_init_status = CAN_F103_InitNormal();

        g_rev_f103_btr = CAN1->BTR;
        g_rev_f103_mcr = CAN1->MCR;

        if (g_rev_f103_init_status == CAN_F103_OK)
        {
            g_rev_f103_ready = 1U;

            /* 3. Prepare one standard CAN data frame */
            const can_f103_frame_t frame = {
                .id = 0x321U,
                .dlc = 1U,
                .data = {0xA5U}
            };

            /* 4. Transmit once */
            g_rev_f103_tx_status =
                CAN_F103_SendPolling(&frame);

            /* 5. Store diagnostic registers */
            g_rev_f103_esr = CAN1->ESR;
            g_rev_f103_tsr = CAN1->TSR;
        }
    }

    g_rev_f103_done = 1U;

    /* Preserve results for debugger */
    while (1)
    {
        __NOP();
    }
}
