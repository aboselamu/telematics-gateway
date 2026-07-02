#include "stm32_it.h"
#include "exti_driver.h"


/* 1. Import handles from your current app (main.c) */
// extern USART_Handle_t myUsart1Handle;
// extern USART_Handle_t myUsart2Handle;
// Note: EXTI doesn't need external handles if your driver uses 
// a private global array to store them, like we designed.





/* --- EXTI INTERRUPT SECTION --- */

void EXTI0_IRQHandler(void) {
    // Dedicated handler for Pin 0
    EXTI_IRQHandling(0);
}

void EXTI1_IRQHandler(void) {
    // Dedicated handler for Pin 1
    EXTI_IRQHandling(1);
}

void EXTI9_5_IRQHandler(void) {
    /**
     * Professional Tip: Lines 5 to 9 share ONE handler. 
     * The driver logic inside EXTI_IRQHandling will figure out 
     * exactly which pin (5, 6, 7, 8, or 9) triggered it.
     */
    for(int i = 5; i <= 9; i++) {
        EXTI_IRQHandling(i);
    }
}
