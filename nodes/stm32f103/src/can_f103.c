#include "stm32f1xx.h"
#include "can_f103.h"
#include <stddef.h>

/* Bounded iteration guards for initial bring-up, NOT calibrated timeouts.
 * Replace with the platform timebase when preparing normal-mode operation.
 */
#define CAN_F103_WAIT_SPINS   (2000000UL)

/* CAN1: PCLK1=36MHz, BRP=4, BS1=15TQ, BS2=2TQ, SJW=1TQ.
 * Bitrate = 36MHz / [4*(1+15+2)] = 500kbit/s.
 * Normal mode drives physical bus; silent loopback does not.
 */
#define CAN_F103_BTR_500K_NORMAL            (0x001E0003UL)
#define CAN_F103_BTR_500K_SILENT_LOOPBACK   (0xC01E0003UL)

static int wait_bits(volatile const uint32_t *reg,
                     uint32_t mask, uint32_t expected)
{
    uint32_t spins = CAN_F103_WAIT_SPINS;
    while (spins-- != 0UL)
    {
        if ((*reg & mask) == expected)
        {
            return 1;
        }
    }
    return 0;
}

static void can_f103_gpio_init(void)
{
    /* CAN1 remap 10b: PB8=RX, PB9=TX. Unlike F446, F103 uses AFIO. */
    RCC->APB2ENR |= RCC_APB2ENR_AFIOEN | RCC_APB2ENR_IOPBEN;

    AFIO->MAPR = (AFIO->MAPR & ~(3UL << 13U)) | (2UL << 13U);

    /* CRH nibble for PB8 = 0x8: input pull-up/pull-down.
     * CRH nibble for PB9 = 0xB: alternate function push-pull 50MHz.
     * MCP2551 has TTL-compatible TXD input (F103 PB9 push-pull is appropriate).
     */
    GPIOB->CRH = (GPIOB->CRH & ~0xFFUL) | 0xB8UL;
    GPIOB->ODR |= (1UL << 8U);  /* PB8 internal pull-up */
}

static can_f103_status_t can_f103_init(uint32_t btr)
{
    can_f103_gpio_init();

    RCC->APB1ENR |= RCC_APB1ENR_CAN1EN;

    /* Establish deterministic bxCAN register state. */
    RCC->APB1RSTR |= RCC_APB1RSTR_CAN1RST;
    RCC->APB1RSTR &= ~RCC_APB1RSTR_CAN1RST;

    CAN1->MCR &= ~CAN_MCR_SLEEP;
    CAN1->MCR |= CAN_MCR_INRQ;

    if (!wait_bits(&CAN1->MSR, CAN_MSR_INAK, CAN_MSR_INAK))
    {
        return CAN_F103_ERR_INIT_TIMEOUT;
    }

    // CAN1->IER = 0UL; /* Polling-only milestone; no CAN ISR required. */
    // CAN1->BTR = btr;

    CAN1->IER = 0UL;

    /* Single-shot transmission during physical CAN testing.
    * Avoid endless automatic retransmission if ACK is missing.
    */
    CAN1->MCR |= CAN_MCR_NART;

    CAN1->BTR = btr;

    /* Filter bank 0: 32-bit mask mode, mask 0, route to FIFO0. */
    CAN1->FMR |= CAN_FMR_FINIT;
    CAN1->FA1R &= ~(1UL << 0U);
    CAN1->FM1R &= ~(1UL << 0U);
    CAN1->FS1R |=  (1UL << 0U);
    CAN1->FFA1R &= ~(1UL << 0U);
    CAN1->sFilterRegister[0].FR1 = 0UL;
    CAN1->sFilterRegister[0].FR2 = 0UL;
    CAN1->FA1R |= (1UL << 0U);
    CAN1->FMR &= ~CAN_FMR_FINIT;

    CAN1->MCR &= ~CAN_MCR_INRQ;
    if (!wait_bits(&CAN1->MSR, CAN_MSR_INAK, 0UL))
    {
        return CAN_F103_ERR_EXIT_INIT_TIMEOUT;
    }
    return CAN_F103_OK;
}

can_f103_status_t CAN_F103_InitSilentLoopback(void)
{
    return can_f103_init(CAN_F103_BTR_500K_SILENT_LOOPBACK);
}

can_f103_status_t CAN_F103_InitNormal(void)
{
    return can_f103_init(CAN_F103_BTR_500K_NORMAL);
}

can_f103_status_t CAN_F103_SendPolling(const can_f103_frame_t *frame)
{
    if ((frame == NULL) || (frame->id > 0x7FFU) || (frame->dlc > 8U))
    {
        return CAN_F103_ERR_PARAM;
    }

    if (!wait_bits(&CAN1->TSR, CAN_TSR_TME0, CAN_TSR_TME0))
    {
        return CAN_F103_ERR_TX_MAILBOX_TIMEOUT;
    }

    /* A previous RQCP0 event must not be mistaken for this request. */
    CAN1->TSR = CAN_TSR_RQCP0;

    CAN1->sTxMailBox[0].TIR  = ((uint32_t)frame->id << 21U);
    CAN1->sTxMailBox[0].TDTR = (uint32_t)frame->dlc;

    uint32_t low = 0UL;
    uint32_t high = 0UL;
    for (uint32_t i = 0U; i < frame->dlc; ++i)
    {
        if (i < 4U)
        {
            low |= ((uint32_t)frame->data[i] << (8U * i));
        }
        else
        {
            high |= ((uint32_t)frame->data[i] << (8U * (i - 4U)));
        }
    }
    CAN1->sTxMailBox[0].TDLR = low;
    CAN1->sTxMailBox[0].TDHR = high;
    CAN1->sTxMailBox[0].TIR |= CAN_TI0R_TXRQ;

    if (!wait_bits(&CAN1->TSR, CAN_TSR_RQCP0, CAN_TSR_RQCP0))
    {
        return CAN_F103_ERR_TX_COMPLETE_TIMEOUT;
    }
    const uint32_t success = (CAN1->TSR & CAN_TSR_TXOK0);
    CAN1->TSR = CAN_TSR_RQCP0; /* Write 1 to clear mailbox-0 completion. */
    return (success != 0UL) ? CAN_F103_OK : CAN_F103_ERR_TX_FAILED;
}

can_f103_status_t CAN_F103_ReceivePolling(can_f103_frame_t *frame)
{
    if (frame == NULL)
    {
        return CAN_F103_ERR_PARAM;
    }

    /* FMP0 is a 2-bit pending-count field; accept 1, 2 or 3 frames. */
    uint32_t spins = CAN_F103_WAIT_SPINS;
    while (((CAN1->RF0R & 0x3UL) == 0UL) && (spins-- != 0UL))
    {
    }
    if ((CAN1->RF0R & 0x3UL) == 0UL)
    {
        return CAN_F103_ERR_RX_TIMEOUT;
    }

    const uint32_t rir = CAN1->sFIFOMailBox[0].RIR;
    const uint32_t rdtr = CAN1->sFIFOMailBox[0].RDTR;
    const uint32_t low = CAN1->sFIFOMailBox[0].RDLR;
    const uint32_t high = CAN1->sFIFOMailBox[0].RDHR;
    const uint32_t dlc = (rdtr & 0xFUL);

    /* Only standard data frames are in scope for this v1 driver. */
    if (((rir & (1UL << 2U)) != 0UL) || /* IDE: extended */
        ((rir & (1UL << 1U)) != 0UL) || /* RTR: remote */
        (dlc > 8UL))
    {
        CAN1->RF0R = CAN_RF0R_RFOM0;
        return CAN_F103_ERR_RX_BAD_DLC;
    }

    frame->id = (uint16_t)((rir >> 21U) & 0x7FFUL);
    frame->dlc = (uint8_t)dlc;
    for (uint32_t i = 0UL; i < 8UL; ++i)
    {
        uint8_t b = 0U;
        if (i < dlc)
        {
            b = (i < 4UL)
              ? (uint8_t)((low >> (8UL * i)) & 0xFFUL)
              : (uint8_t)((high >> (8UL * (i - 4UL))) & 0xFFUL);
        }
        frame->data[i] = b;
    }

    CAN1->RF0R = CAN_RF0R_RFOM0; /* release FIFO0 output mailbox */
    return CAN_F103_OK;
}
