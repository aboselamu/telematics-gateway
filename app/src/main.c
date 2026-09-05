#include "stm32f446xx.h"

volatile uint16_t rx_id   = 0U;
volatile uint8_t  rx_dlc  = 0U;
volatile uint8_t  rx_data = 0U;

/*=============================================================================
 * System Clock
 *============================================================================*/
void SystemClock_Config(void)
{
    RCC->CR |= RCC_CR_HSION;

    while ((RCC->CR & RCC_CR_HSIRDY) == 0U)
    {
    }

    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    PWR->CR |= PWR_CR_VOS;

    FLASH->ACR =
        FLASH_ACR_ICEN |
        FLASH_ACR_DCEN |
        FLASH_ACR_LATENCY_5WS;

    RCC->PLLCFGR =
        (16U  << RCC_PLLCFGR_PLLM_Pos) |
        (360U << RCC_PLLCFGR_PLLN_Pos) |
        (0U   << RCC_PLLCFGR_PLLP_Pos) |
        RCC_PLLCFGR_PLLSRC_HSI;

    RCC->CR |= RCC_CR_PLLON;

    while ((RCC->CR & RCC_CR_PLLRDY) == 0U)
    {
    }

    RCC->CFGR |=
        RCC_CFGR_HPRE_DIV1 |
        RCC_CFGR_PPRE1_DIV4 |
        RCC_CFGR_PPRE2_DIV2;

    RCC->CFGR |= RCC_CFGR_SW_PLL;

    while ((RCC->CFGR & RCC_CFGR_SWS) !=
           RCC_CFGR_SWS_PLL)
    {
    }
}

void Gpio_init(void){
        /* GPIOB clock */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;

    /* PB8 = Alternate Function */
    GPIOB->MODER &= ~(3U << 16U);
    GPIOB->MODER |=  (2U << 16U);

    /* PB8 = AF9 = CAN1_RX */
    GPIOB->AFR[1] &= ~(0xFU << 0U);
    GPIOB->AFR[1] |=  (9U << 0U);

    /* PB8 pull-up: recessive CAN_RX */
    GPIOB->PUPDR &= ~(3U << 16U);
    GPIOB->PUPDR |=  (1U << 16U);
}

int main(void)
{
    SystemClock_Config();

    /* 1. Enable CAN1 clock */
    RCC->APB1ENR |= RCC_APB1ENR_CAN1EN;

    // Egziabiher ka'ene gar nawu= AMEN
    Gpio_init();

    /* 2. Leave sleep mode */
    CAN1->MCR &= ~CAN_MCR_SLEEP;

    /* 3. Request initialization mode */
    CAN1->MCR |= CAN_MCR_INRQ;

    /* Wait until bxCAN acknowledges INIT mode */
    while ((CAN1->MSR & CAN_MSR_INAK) == 0U)
    {

    }

    /*
     * 4. Configure 500 kbit/s
     *
     * CAN clock = 45 MHz
     *
     * BRP = 5  -> register value 4
     * BS1 = 15 -> register value 14
     * BS2 = 2  -> register value 1
     * SJW = 1  -> register value 0
     *
     * Silent + Loopback enabled
     */
    CAN1->BTR =
          (4U  << 0U)
        | (14U << 16U)
        | (1U  << 20U)
        | (0U  << 24U)
        | CAN_BTR_LBKM
        | CAN_BTR_SILM;


    /* 5. Configure filter bank 0: accept all */

    /* Enter filter initialization mode */
    CAN1->FMR |= CAN_FMR_FINIT;

    /* Disable filter bank 0 while configuring */
    CAN1->FA1R &= ~CAN_FA1R_FACT0;
    /* Mask mode: 0 = mask mode */
    CAN1->FM1R &= ~CAN_FM1R_FBM0;

    /* 32-bit scale: 1 = 32-bit */
    CAN1->FS1R |= CAN_FS1R_FSC0;

    /* Assign bank 0 to FIFO0: 0 = FIFO0 */
    CAN1->FFA1R &= ~CAN_FFA1R_FFA0;

    /* Accept everything */
    CAN1->sFilterRegister[0].FR1 = 0x00000000U;
    CAN1->sFilterRegister[0].FR2 = 0x00000000U;

    /* Activate filter bank 0 */
    CAN1->FA1R |= CAN_FA1R_FACT0;

    /* Leave filter initialization mode */
    CAN1->FMR &= ~CAN_FMR_FINIT;



    /* 6. Leave initialization mode */
    CAN1->MCR &= ~CAN_MCR_INRQ;

    /* Wait until bxCAN leaves INIT mode */
    while ((CAN1->MSR & CAN_MSR_INAK) != 0U)
    {
    }
    //d 
    /* Test 3: Load TX mailbox 0 only */
    /* Check mailbox 0 is empty */
    while ((CAN1->TSR & CAN_TSR_TME0) == 0U)
    {
    }

    /* Standard ID = 0x123, IDE = 0, RTR = 0, TXRQ = 0 */
    CAN1->sTxMailBox[0].TIR =
        (0x123U << CAN_TI0R_STID_Pos);

    /* DLC = 1 */
    CAN1->sTxMailBox[0].TDTR = 1U;

    /* DATA[0] = 0x5A */
    CAN1->sTxMailBox[0].TDLR = 0x5AU;

    /* No upper data bytes */
    CAN1->sTxMailBox[0].TDHR = 0U;

    /* Request transmission */
    CAN1->sTxMailBox[0].TIR |= CAN_TI0R_TXRQ;


    /* Read received frame */
    rx_id =
        (uint16_t)((CAN1->sFIFOMailBox[0].RIR &
                    CAN_RI0R_STID_Msk)
                    >> CAN_RI0R_STID_Pos);

    rx_dlc =
        (uint8_t)(CAN1->sFIFOMailBox[0].RDTR &
                CAN_RDT0R_DLC_Msk);

    rx_data =
        (uint8_t)(CAN1->sFIFOMailBox[0].RDLR &
                0xFFU);

    /* Release FIFO0 */
    CAN1->RF0R = CAN_RF0R_RFOM0;

    /* Stop */
    while (1)
    {
    }

}

// Check this out
// p/x ((CAN1->RF0R & CAN_RF0R_FMP0_Msk) >> CAN_RF0R_FMP0_Pos)
// p/x CAN1->RF0R