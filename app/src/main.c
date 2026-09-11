#include "stm32f446xx.h"
#include "can.h"
#include <stddef.h>
#include "timebase.h"

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

int main(void)
{
    SystemClock_Config();

    Timebase_Init();
    volatile uint32_t t1 = millis();
    for (volatile uint32_t i = 0U; i < 1000000U; i++)
    {
    }

    volatile uint32_t t2 = millis();

    // CAN_frame_t rx_cframe = {0};
    // CAN_frame_t tx_cframe;
    // volatile CAN_Status_t tx_status;
    // volatile CAN_Status_t rx_status;

    // tx_cframe.id = 0x123U;
    // tx_cframe.dlc = 8U;

    // tx_cframe.data[0] = 0x11U;
    // tx_cframe.data[1] = 0x22U;
    // tx_cframe.data[2] = 0x33U;
    // tx_cframe.data[3] = 0x44U;
    // tx_cframe.data[4] = 0x55U;
    // tx_cframe.data[5] = 0x66U;
    // tx_cframe.data[6] = 0x77U;
    // tx_cframe.data[7] = 0x88U;

    // CAN1_Init();

    // tx_status = CAN1_SendPolling(&tx_cframe);
   
    // rx_status = CAN1_ReceivePolling(&rx_cframe);

    /* Stop */
    while (1)
    {
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