#ifndef STM32_IT_H
#define STM32_IT_H
#include "stm32_it.h"     // Define your IRQ prototypes here
#include "exti_driver.h"  // Required for EXTI_IRQHandling

// ... existing ISRs ...
//void USART2_IRQHandler(void);
void EXTI0_IRQHandler(void);
void EXTI9_5_IRQHandler(void);
 
#endif