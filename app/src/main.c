#include "stm32f4xx.h"
#include "uart_driver.h"
#include "event_queue.h"
#include <string.h>

#define APP_RX_BUFFER_SIZE 256
uint8_t app_rx_buffer[APP_RX_BUFFER_SIZE];

void SystemClock_Config(void) {
    RCC->CR |= RCC_CR_HSION;
    while((RCC->CR & RCC_CR_HSIRDY) == 0);

    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    PWR->CR |= PWR_CR_VOS; 

    FLASH->ACR = FLASH_ACR_ICEN | FLASH_ACR_DCEN | FLASH_ACR_LATENCY_5WS;

    RCC->PLLCFGR = (16 << RCC_PLLCFGR_PLLM_Pos) | 
                   (360 << RCC_PLLCFGR_PLLN_Pos) | 
                   (0 << RCC_PLLCFGR_PLLP_Pos)   | 
                   RCC_PLLCFGR_PLLSRC_HSI;

    RCC->CR |= RCC_CR_PLLON;
    while((RCC->CR & RCC_CR_PLLRDY) == 0);

    RCC->CFGR |= RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE1_DIV4 | RCC_CFGR_PPRE2_DIV2;
    RCC->CFGR |= RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);
}

int main(void) {
    SystemClock_Config();
    eventQueue_init();
    
    uart_config_t uart_cfg = { .baud_rate = 115200 };
    uart_init(&uart_cfg);

    while (1) {
        event_t current_event;

        if (eventQueue_poll(&current_event) == EVENT_QUEUE_OK) {
            if (current_event.event_id == EVT_UART_FRAME_READY) {
                uint16_t claim_ticket = current_event.param1;
                uint16_t len = uart_read(app_rx_buffer, APP_RX_BUFFER_SIZE, claim_ticket);
                
                if (len > 0) {
                    uart_send((const uint8_t *)app_rx_buffer, len);
                }
            }
        }
    }
}
