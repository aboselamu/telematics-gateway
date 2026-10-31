#include "stm32f446xx.h"
#include "can_driver.h"
#include "timebase.h"
#include <stddef.h>

/* Preserves the verified standalone F446 bit timing:
 * PCLK1 = 45 MHz; BRP=5, BS1=15, BS2=2, SJW=1 => 500 kbit/s.
 */
#define CAN_TX_MAILBOX_TIMEOUT_MS   5U
#define CAN_TX_COMPLETE_TIMEOUT_MS  5U
#define CAN_INIT_SPIN_LIMIT         1000000UL
#define CAN_RX_QUEUE_SIZE          8U

/* Single producer: CAN1_RX0_IRQHandler.
 * Single consumer: foreground CAN1_Read.
 * Ring capacity = 7 frames (one slot distinguishes full from empty).
 */
static volatile uint8_t s_rx_head;
static volatile uint8_t s_rx_tail;
static volatile CAN_frame_t s_rx_queue[CAN_RX_QUEUE_SIZE];
static volatile uint32_t s_rx_drops;
static CAN_RxCallback_t s_rx_callback = NULL;

void CAN1_RegisterRxCallback(CAN_RxCallback_t callback)
{
    /* Caller configures this before CAN interrupts are enabled. */
    s_rx_callback = callback;
}

static void can1_gpio_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;

    /* PB8 -> CAN1_RX / AF9; weak 3.3 V pull-up for recessive level. */
    GPIOB->MODER = (GPIOB->MODER & ~(3U << 16U)) | (2U << 16U);
    GPIOB->AFR[1] = (GPIOB->AFR[1] & ~(0xFU << 0U)) | (9U << 0U);
    GPIOB->PUPDR = (GPIOB->PUPDR & ~(3U << 16U)) | (1U << 16U);

    /* PB9 -> CAN1_TX / AF9, open-drain for the external 5 V pull-up.
     * Hardware caution: keep PB9 DISCONNECTED from TD521DCAN TXD until
     * OTYPER bit 9 and the board-level electrical compatibility are verified.
     */
    GPIOB->OTYPER |= (1U << 9U);
    GPIOB->MODER = (GPIOB->MODER & ~(3U << 18U)) | (2U << 18U);
    GPIOB->AFR[1] = (GPIOB->AFR[1] & ~(0xFU << 4U)) | (9U << 4U);
    GPIOB->PUPDR &= ~(3U << 18U);
}

/* Bounded spin for peripheral init transitions: timebase may not yet run. */
static int can1_wait_inak(uint32_t expected_set)
{
    for (uint32_t i = 0U; i < CAN_INIT_SPIN_LIMIT; ++i)
    {
        uint32_t set = ((CAN1->MSR & CAN_MSR_INAK) != 0U) ? 1U : 0U;
        if (set == expected_set)
        {
            return 1;
        }
    }
    return 0;
}

int CAN1_Init(void)
{
    return (int)CAN1_InitMode(CAN_MODE_SILENT_LOOPBACK);
}

CAN_Status_t CAN1_InitMode(CAN_Mode_t mode)
{
    if (mode != CAN_MODE_SILENT_LOOPBACK && mode != CAN_MODE_NORMAL)
    {
        return CAN_ERROR_TX;
    }

    RCC->APB1ENR |= RCC_APB1ENR_CAN1EN;
    can1_gpio_init();

    NVIC_DisableIRQ(CAN1_RX0_IRQn);
    NVIC_DisableIRQ(CAN1_TX_IRQn);
    CAN1->IER &= ~(CAN_IER_FMPIE0 | CAN_IER_TMEIE);

    s_rx_head = 0U;
    s_rx_tail = 0U;
    s_rx_drops = 0U;

    CAN1->MCR &= ~CAN_MCR_SLEEP;
    CAN1->MCR |= CAN_MCR_INRQ;
    if (!can1_wait_inak(1U))
    {
        return CAN_ERROR_INIT_TIMEOUT;
    }

    CAN1->BTR =
          (4U  << 0U)       /* BRP = 5 */
        | (14U << 16U)      /* BS1 = 15 */
        | (1U  << 20U);     /* BS2 = 2; SJW = 1 */

    if (mode == CAN_MODE_SILENT_LOOPBACK)
    {
        CAN1->BTR |= CAN_BTR_LBKM | CAN_BTR_SILM;
    }

    /* Filter bank 0: 32-bit, mask mode, FIFO0, accept all. */
    CAN1->FMR |= CAN_FMR_FINIT;
    CAN1->FA1R &= ~CAN_FA1R_FACT0;
    CAN1->FM1R &= ~CAN_FM1R_FBM0;
    CAN1->FS1R |= CAN_FS1R_FSC0;
    CAN1->FFA1R &= ~CAN_FFA1R_FFA0;
    CAN1->sFilterRegister[0].FR1 = 0U;
    CAN1->sFilterRegister[0].FR2 = 0U;
    CAN1->FA1R |= CAN_FA1R_FACT0;
    CAN1->FMR &= ~CAN_FMR_FINIT;

    /* Configure one-shot transmission while still in initialization mode. */
    if (mode == CAN_MODE_NORMAL)
    {
        CAN1->MCR |= CAN_MCR_NART;
    }
    else
    {
        CAN1->MCR &= ~CAN_MCR_NART;
    }

    CAN1->MCR &= ~CAN_MCR_INRQ;
    if (!can1_wait_inak(0U))
    {
        return CAN_ERROR_INIT_TIMEOUT;
    }

    /* RX interrupt only. TX completion is owned by the polling API.
     * Do not enable CAN1_TX_IRQn until an actual async TX design exists.
     */
    CAN1->IER |= CAN_IER_FMPIE0;
    NVIC_ClearPendingIRQ(CAN1_RX0_IRQn);
    NVIC_EnableIRQ(CAN1_RX0_IRQn);
    return CAN_OK;
}

CAN_Status_t CAN1_SendPolling(const CAN_frame_t *frame)
{
    if (frame == NULL)
    {
        return CAN_ERROR_NULL_PTR;
    }
    if (frame->dlc > 8U)
    {
        return CAN_ERROR_INVALID_DLC;
    }
    if (frame->id > 0x7FFU)
    {
        return CAN_ERROR_INVALID_ID;
    }

    /* Requires an initialized, monotonically ticking millis() timebase. */
    uint32_t t_start = millis();
    while ((CAN1->TSR & CAN_TSR_TME0) == 0U)
    {
        if ((uint32_t)(millis() - t_start) >= CAN_TX_MAILBOX_TIMEOUT_MS)
        {
            return CAN_ERROR_TX_MAILBOX_TIMEOUT;
        }
    }

    CAN1->sTxMailBox[0].TIR = ((uint32_t)frame->id << CAN_TI0R_STID_Pos);
    CAN1->sTxMailBox[0].TDTR = (uint32_t)frame->dlc;

    uint32_t low = 0U;
    uint32_t high = 0U;
    for (uint32_t i = 0U; i < frame->dlc; ++i)
    {
        if (i < 4U)
        {
            low |= ((uint32_t)frame->data[i] << (i * 8U));
        }
        else
        {
            high |= ((uint32_t)frame->data[i] << ((i - 4U) * 8U));
        }
    }
    CAN1->sTxMailBox[0].TDLR = low;
    CAN1->sTxMailBox[0].TDHR = high;

    CAN1->sTxMailBox[0].TIR |= CAN_TI0R_TXRQ;

    t_start = millis();
    while ((CAN1->TSR & CAN_TSR_RQCP0) == 0U)
    {
        if ((uint32_t)(millis() - t_start) >= CAN_TX_COMPLETE_TIMEOUT_MS)
        {
            /* Do not automatically clear TXRQ or infer TX success here.
             * In normal mode with no ACK, bxCAN may retry indefinitely.
             * Recovery/abort policy belongs to CAN hardening stage.
             */
            return CAN_ERROR_TX_COMPLETE_TIMEOUT;
        }
    }

    uint32_t success = (CAN1->TSR & CAN_TSR_TXOK0);
    CAN1->TSR = CAN_TSR_RQCP0;  /* RQCP0 is write-one-to-clear. */
    return (success != 0U) ? CAN_OK : CAN_ERROR_TX;
}

CAN_Status_t CAN1_Read(CAN_frame_t *frame)
{
    if (frame == NULL)
    {
        return CAN_ERROR_NULL_PTR;
    }
    if (s_rx_tail == s_rx_head)
    {
        return CAN_ERROR_RX_EMPTY;
    }

    const uint8_t tail = s_rx_tail;
    frame->id = s_rx_queue[tail].id;
    frame->dlc = s_rx_queue[tail].dlc;
    for (uint32_t i = 0U; i < frame->dlc; ++i)
    {
        frame->data[i] = s_rx_queue[tail].data[i];
    }
    for (uint32_t i = frame->dlc; i < 8U; ++i)
    {
        frame->data[i] = 0U;
    }
    s_rx_tail = (uint8_t)((tail + 1U) % CAN_RX_QUEUE_SIZE);
    return CAN_OK;
}

uint32_t CAN1_RxDropCount(void)
{
    return s_rx_drops;
}

void CAN1_RX0_IRQHandler(void)
{
    /* Drain pending hardware FIFO messages in one IRQ. */
    while ((CAN1->RF0R & CAN_RF0R_FMP0_Msk) != 0U)
    {
        uint8_t head = s_rx_head;
        uint8_t next = (uint8_t)((head + 1U) % CAN_RX_QUEUE_SIZE);
        if (next == s_rx_tail)
        {
            ++s_rx_drops;
            CAN1->RF0R = CAN_RF0R_RFOM0;
            continue;
        }

        uint32_t rir = CAN1->sFIFOMailBox[0].RIR;
        uint32_t rdtr = CAN1->sFIFOMailBox[0].RDTR;
        uint32_t low = CAN1->sFIFOMailBox[0].RDLR;
        uint32_t high = CAN1->sFIFOMailBox[0].RDHR;
        uint32_t dlc = rdtr & CAN_RDT0R_DLC_Msk;

        /* Scope: standard 11-bit data frames only. */
        if (((rir & (CAN_RI0R_IDE | CAN_RI0R_RTR)) != 0U) || dlc > 8U)
        {
            ++s_rx_drops;
            CAN1->RF0R = CAN_RF0R_RFOM0;
            continue;
        }

        s_rx_queue[head].id =
            (uint16_t)((rir & CAN_RI0R_STID_Msk) >> CAN_RI0R_STID_Pos);
        s_rx_queue[head].dlc = (uint8_t)dlc;

        for (uint32_t i = 0U; i < dlc; ++i)
        {
            uint32_t word = (i < 4U) ? low : high;
            uint32_t shift = ((i < 4U) ? i : (i - 4U)) * 8U;
            s_rx_queue[head].data[i] = (uint8_t)((word >> shift) & 0xFFU);
        }
        s_rx_head = next; /* publish only after frame was copied completely */
        CAN1->RF0R = CAN_RF0R_RFOM0;

        /* Notify upper layer after committing frame, while still in IRQ. */
        if (s_rx_callback != NULL)
        {
            s_rx_callback();
        }
    }
}
