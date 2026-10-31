#include "stm32f1xx.h"
#include "clock_f103.h"

/* CMSIS-compatible variable: set only after a verified clock transition. */
uint32_t SystemCoreClock = 8000000UL;

#define CLOCK_WAIT_LIMIT  (3000000UL)

/* Startup hook; our vector table resides at 0x08000000. */
void SystemInit(void)
{
    SCB->VTOR = FLASH_BASE;
}

static int wait_for_bits(volatile const uint32_t *reg,
                         uint32_t mask,
                         uint32_t expected)
{
    uint32_t n = CLOCK_WAIT_LIMIT;
    while (n-- != 0UL)
    {
        if ((*reg & mask) == expected)
        {
            return 1;
        }
    }
    return 0;
}

clock_f103_status_t ClockF103_Init72MHz(void)
{
    /* Startup reset clock is HSI 8 MHz. */
    /* External 8 MHz crystal; not HSE bypass. */
    RCC->CR &= ~RCC_CR_HSEBYP;
    RCC->CR |= RCC_CR_HSEON;
    if (!wait_for_bits(&RCC->CR, RCC_CR_HSERDY, RCC_CR_HSERDY))
    {
        return CLOCK_F103_ERR_HSE_TIMEOUT;
    }

    /* Two wait states are required before increasing SYSCLK to 72 MHz. */
    FLASH->ACR = (FLASH->ACR & ~FLASH_ACR_LATENCY)
               | (2U << FLASH_ACR_LATENCY_Pos) | FLASH_ACR_PRFTBE;

    /* HSE 8 MHz * 9 = SYSCLK 72 MHz.
       AHB 72 MHz, APB1 36 MHz (CAN), APB2 72 MHz. */
    RCC->CFGR &= ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2 | RCC_CFGR_ADCPRE
                |  RCC_CFGR_PLLSRC | RCC_CFGR_PLLXTPRE | RCC_CFGR_PLLMULL);
    /* ADC clock remains <= 14 MHz if ADC is enabled later: PCLK2 / 6 = 12 MHz. */
    RCC->CFGR |= (RCC_CFGR_PPRE1_DIV2 | RCC_CFGR_ADCPRE_DIV6
               |  RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL9);

    RCC->CR |= RCC_CR_PLLON;
    if (!wait_for_bits(&RCC->CR, RCC_CR_PLLRDY, RCC_CR_PLLRDY))
    {
        return CLOCK_F103_ERR_PLL_TIMEOUT;
    }

    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
    if (!wait_for_bits(&RCC->CFGR, RCC_CFGR_SWS, RCC_CFGR_SWS_PLL))
    {
        return CLOCK_F103_ERR_SWITCH_TIMEOUT;
    }

    SystemCoreClock = 72000000UL;
    return CLOCK_F103_OK;
}
