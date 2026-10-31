#include "stm32f446xx.h"
#include "can_driver.h"
#include <stddef.h>
#include "timebase.h"
#include "event_queue.h"

volatile int g_can_init_result;
volatile CAN_Status_t g_can_tx_status;
volatile CAN_Status_t g_can_rx_status;
CAN_frame_t g_can_received_frame;

#include "can_driver.h"
#include "stm32f446xx.h"

volatile CAN_Status_t g_bus_f446_init_status = CAN_ERROR_INIT_TIMEOUT;
volatile CAN_Status_t g_bus_f446_tx_status = CAN_ERROR_TX;
volatile uint32_t g_bus_f446_done = 0U;
volatile uint32_t g_bus_f446_btr = 0U;
volatile uint32_t g_bus_f446_esr = 0U;
volatile uint32_t g_bus_f446_tsr = 0U;

volatile uint32_t g_rev_ready = 0U;
volatile uint32_t g_rev_done = 0U;
volatile uint32_t g_rev_pass = 0U;

volatile CAN_Status_t g_rev_init_status = CAN_ERROR_INIT_TIMEOUT;
volatile CAN_Status_t g_rev_rx_status = CAN_ERROR_RX_EMPTY;

volatile uint32_t g_rev_btr = 0U;
volatile uint32_t g_rev_rx_id = 0U;
volatile uint32_t g_rev_rx_dlc = 0U;
volatile uint32_t g_rev_rx_data0 = 0U;
volatile uint32_t g_rev_esr = 0U;
volatile uint32_t g_rev_rx_drops = 0U;

volatile uint32_t g_multi_ready = 0U;
volatile uint32_t g_multi_done = 0U;

volatile uint32_t g_multi_received = 0U;
volatile uint32_t g_multi_pass = 0U;
volatile uint32_t g_multi_bad = 0U;

volatile CAN_Status_t g_multi_init_status =
    CAN_ERROR_INIT_TIMEOUT;

volatile uint32_t g_multi_btr = 0U;
volatile uint32_t g_multi_esr = 0U;
volatile uint32_t g_multi_drops = 0U;

/*=============================================================================
 * System Clock
 *============================================================================*/
void SystemClock_Config(void)
{
    /*--------------------------------------------------------------
     * 1. Enable HSI
     *
     * HSI = 16 MHz
     *-------------------------------------------------------------*/
    RCC->CR |= RCC_CR_HSION;

    while ((RCC->CR & RCC_CR_HSIRDY) == 0U)
    {
    }


    /*--------------------------------------------------------------
     * 2. Enable PWR peripheral clock
     *-------------------------------------------------------------*/
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;


    /*--------------------------------------------------------------
     * 3. Select Voltage Scale 1
     *
     * VOS[1:0] = 11
     *
     * Scale 1 + Over-drive allows 180 MHz.
     *
     * Configure this while PLL is OFF.
     *-------------------------------------------------------------*/
    PWR->CR &= ~PWR_CR_VOS_Msk;
    PWR->CR |= (3U << PWR_CR_VOS_Pos);


    /*--------------------------------------------------------------
     * 4. Configure Flash
     *
     * Nucleo supply ≈ 3.3 V
     * HCLK = 180 MHz
     * => 5 wait states
     *-------------------------------------------------------------*/
    FLASH->ACR =
          FLASH_ACR_ICEN
        | FLASH_ACR_DCEN
        | FLASH_ACR_PRFTEN
        | FLASH_ACR_LATENCY_5WS;


    /*--------------------------------------------------------------
     * 5. Make sure PLL is OFF before configuration
     *-------------------------------------------------------------*/
    RCC->CR &= ~RCC_CR_PLLON;

    while ((RCC->CR & RCC_CR_PLLRDY) != 0U)
    {
    }


    /*--------------------------------------------------------------
     * 6. Configure main PLL
     *
     * HSI = 16 MHz
     *
     * PLL input:
     *      16 MHz / PLLM(16)
     *      = 1 MHz
     *
     * VCO:
     *      1 MHz * PLLN(360)
     *      = 360 MHz
     *
     * SYSCLK:
     *      360 MHz / PLLP(2)
     *      = 180 MHz
     *
     * PLLQ = 8 -> 45 MHz
     * PLLR = 2 -> valid PLLR setting
     *
     * PLLQ is NOT producing 48 MHz here,
     * so this configuration is not intended
     * for USB FS clock generation.
     *-------------------------------------------------------------*/
    RCC->PLLCFGR =
          (16U  << RCC_PLLCFGR_PLLM_Pos)
        | (360U << RCC_PLLCFGR_PLLN_Pos)
        | (0U   << RCC_PLLCFGR_PLLP_Pos)   /* PLLP = 2 */
        | (8U   << RCC_PLLCFGR_PLLQ_Pos)
        | (2U   << RCC_PLLCFGR_PLLR_Pos)
        | RCC_PLLCFGR_PLLSRC_HSI;


    /*--------------------------------------------------------------
     * 7. Enable PLL
     *-------------------------------------------------------------*/
    RCC->CR |= RCC_CR_PLLON;

    while ((RCC->CR & RCC_CR_PLLRDY) == 0U)
    {
    }


    /*--------------------------------------------------------------
     * 8. Enable Over-drive
     *-------------------------------------------------------------*/
    PWR->CR |= PWR_CR_ODEN;

    while ((PWR->CSR & PWR_CSR_ODRDY) == 0U)
    {
    }


    /*--------------------------------------------------------------
     * 9. Switch regulator to Over-drive mode
     *-------------------------------------------------------------*/
    PWR->CR |= PWR_CR_ODSWEN;

    while ((PWR->CSR & PWR_CSR_ODSWRDY) == 0U)
    {
    }


    /*--------------------------------------------------------------
     * 10. Configure bus prescalers
     *
     * SYSCLK = 180 MHz
     *
     * AHB:
     *      HCLK  = 180 / 1 = 180 MHz
     *
     * APB1:
     *      PCLK1 = 180 / 4 = 45 MHz
     *
     * APB2:
     *      PCLK2 = 180 / 2 = 90 MHz
     *-------------------------------------------------------------*/
    RCC->CFGR &=
        ~(RCC_CFGR_HPRE |
          RCC_CFGR_PPRE1 |
          RCC_CFGR_PPRE2);

    RCC->CFGR |=
          RCC_CFGR_HPRE_DIV1
        | RCC_CFGR_PPRE1_DIV4
        | RCC_CFGR_PPRE2_DIV2;


    /*--------------------------------------------------------------
     * 11. Select PLL as system clock
     *-------------------------------------------------------------*/
    RCC->CFGR &= ~RCC_CFGR_SW;
    RCC->CFGR |= RCC_CFGR_SW_PLL;

    while ((RCC->CFGR & RCC_CFGR_SWS) !=
           RCC_CFGR_SWS_PLL)
    {
    }


    /* Keep CMSIS SystemCoreClock variable correct */
    // SystemCoreClockUpdate();
}






/* CAN event HIL: F103 #1 sends ID=0x321, DLC=1, DATA[0]=0xA5
 * five times. F446 receives via ISR -> ring buffer -> event queue.
 */
#define CAN_EVENT_TEST_FRAMES 5U

volatile CAN_Status_t g_can_ev_init_status = CAN_ERROR_INIT_TIMEOUT;
volatile uint32_t g_can_ev_ready = 0U;
volatile uint32_t g_can_ev_done = 0U;
volatile uint32_t g_can_ev_pass = 0U;
volatile uint32_t g_can_ev_btr = 0U;
volatile uint32_t g_can_ev_received = 0U;
volatile uint32_t g_can_ev_valid = 0U;
volatile uint32_t g_can_ev_invalid = 0U;
volatile uint32_t g_can_ev_events_dispatched = 0U;
volatile uint32_t g_can_ev_post_failures = 0U;
volatile uint32_t g_can_ev_recoveries = 0U;
volatile uint32_t g_can_ev_rx_drops = 0U;
volatile uint32_t g_can_ev_esr = 0U;

/* ISR-context callback: notification only, no CAN frame parsing here. */
static void can_rx_notify(void)
{
    const event_t evt = {
        .event_id = EVT_CAN_RX_READY,
        .timestamp = millis(),
        .param1 = 0U,
        .param2 = 0U
    };

    if (eventQueue_post(&evt) != EVENT_QUEUE_OK)
    {
        ++g_can_ev_post_failures;
    }
}

/* Runs in main context, not inside the CAN interrupt. */
static void can_process_queued_frames(void)
{
    CAN_frame_t frame;

    while (CAN1_Read(&frame) == CAN_OK)
    {
        ++g_can_ev_received;
        if ((frame.id == 0x321U) &&
            (frame.dlc == 1U) &&
            (frame.data[0] == 0xA5U))
        {
            ++g_can_ev_valid;
        }
        else
        {
            ++g_can_ev_invalid;
        }
    }
}

static void gateway_dispatch_event(const event_t *evt)
{
    switch (evt->event_id)
    {
        case EVT_CAN_RX_READY:
            ++g_can_ev_events_dispatched;
            can_process_queued_frames();
            break;

        case EVT_UART_FRAME_READY:
        case EVT_I2C_TRANSFER_DONE:
            /* UART and I2C are not active in this CAN-only HIL image.
             * Future gateway dispatcher will route these to services.
             */
            break;

        default:
            break;
    }
}

int main(void)
{
    SystemClock_Config();
    Timebase_Init();

    /* Queue must exist before ISR can post. */
    (void)eventQueue_init();
    CAN1_RegisterRxCallback(can_rx_notify);

    g_can_ev_init_status = CAN1_InitMode(CAN_MODE_NORMAL);
    g_can_ev_btr = CAN1->BTR;

    if (g_can_ev_init_status == CAN_OK)
    {
        g_can_ev_ready = 1U;
        uint32_t post_failures_seen = 0U;

        while (g_can_ev_received < CAN_EVENT_TEST_FRAMES)
        {
            event_t evt;
            if (eventQueue_poll(&evt) == EVENT_QUEUE_OK)
            {
                gateway_dispatch_event(&evt);
            }
            else if (g_can_ev_post_failures != post_failures_seen)
            {
                /* Recover unannounced frames if event queue was full. */
                post_failures_seen = g_can_ev_post_failures;
                ++g_can_ev_recoveries;
                can_process_queued_frames();
            }
        }
    }

    g_can_ev_rx_drops = CAN1_RxDropCount();
    g_can_ev_esr = CAN1->ESR;
    g_can_ev_pass = (g_can_ev_init_status == CAN_OK &&
                     g_can_ev_received == CAN_EVENT_TEST_FRAMES &&
                     g_can_ev_valid == CAN_EVENT_TEST_FRAMES &&
                     g_can_ev_invalid == 0U &&
                     g_can_ev_events_dispatched != 0U &&
                     g_can_ev_post_failures == 0U &&
                     g_can_ev_rx_drops == 0U) ? 1U : 0U;
    g_can_ev_done = 1U;

    while (1)
    {
        __NOP();
    }
}

// Check this out
// whether the driver should zero the unused bytes.
// deliberately are not linking the CMSIS system implementation right now
// p/x ((CAN1->RF0R & CAN_RF0R_FMP0_Msk) >> CAN_RF0R_FMP0_Pos)
// p/x CAN1->RF0R

// =============================================
/*
HSI = 16 MHz
   │
  /16
   ↓
1 MHz PLL input
   │
  ×360
   ↓
360 MHz VCO
   │
  /2
   ↓
SYSCLK = 180 MHz
   │
   ├── AHB /1  → HCLK  = 180 MHz
   │
   ├── APB1 /4 → PCLK1 = 45 MHz  ← CAN1
   │
   └── APB2 /2 → PCLK2 = 90 MHz
*/