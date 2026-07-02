#include "stm32f4xx.h"

void SystemInit(void) {
    /* Set CP10 and CP11 Full Access to enable the hardware FPU */
    SCB->CPACR |= ((3UL << 10*2)|(3UL << 11*2));  
    
    /* Reset the RCC clock configuration to the default reset state */
    RCC->CR |= (uint32_t)0x00000001; // Enable HSI
    RCC->CFGR = 0x00000000;          // Reset CFGR
    RCC->CR &= (uint32_t)0xFEF6FFFF; // Disable HSE, CSS, PLL
    RCC->PLLCFGR = 0x24003010;       // Reset PLLCFGR
}
