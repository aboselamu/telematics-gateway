#include "stm32f446xx.h"
#include "timebase.h"

static volatile uint32_t msTicks = 0U;

uint32_t millis(void)
{
    return msTicks;
}

void Timebase_Init(void){

       SysTick->LOAD = 180000U - 1U;

       SysTick->VAL  = 0U;

       SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;

}

void SysTick_Handler(void)
{
    msTicks++;
}
