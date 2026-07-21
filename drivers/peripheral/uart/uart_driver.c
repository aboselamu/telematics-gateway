/**
 * @file uart_driver.c
 * @brief Private implementation of the non-blocking UART4 driver.
 * Implements a dual circular ring buffer architecture with DMA-driven reception 
 * and polled processing.
 */

#include "uart_driver.h"
#include "event_queue.h"
#include "dma_driver.h" 
#include "frame_manager.h"
#include "stm32f4xx.h"  
#include <stddef.h>

/* Buffer Sizes must be a power of two for efficient bitwise wrapping mask */
#define RX_BUFFER_SIZE 256
#define TX_BUFFER_SIZE 256

#define RX_BUFFER_MASK (RX_BUFFER_SIZE - 1)
#define TX_BUFFER_MASK (TX_BUFFER_SIZE - 1)

/* Private Internal Driver State Storage */
static uint8_t  s_rx_buffer[RX_BUFFER_SIZE];
// s_rx_head removed: DMA is the sole producer
static volatile uint16_t s_rx_tail = 0;

static uint8_t  s_tx_buffer[TX_BUFFER_SIZE];
static volatile uint16_t s_tx_head = 0;
static volatile uint16_t s_tx_tail = 0;


uart_status_t uart_init(const uart_config_t *p_config) {
    if (p_config == NULL) {
        return UART_ERR_PARAM;
    }

    // Reset internal state indexes
    s_rx_tail = 0;   
    s_tx_head = 0;   
    s_tx_tail = 0;   

    // 1. Clock Configuration (Targeting UART4 on APB1, GPIOA on AHB1)
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_UART4EN;

    // 2. GPIO Pin Configuration (PA0 = TX, PA1 = RX)
    GPIOA->MODER &= ~(GPIO_MODER_MODER0 | GPIO_MODER_MODER1);
    GPIOA->MODER |= (GPIO_MODER_MODER0_1 | GPIO_MODER_MODER1_1);

    GPIOA->AFR[0] &= ~((0xF << (0 * 4)) | (0xF << (1 * 4))); 
    GPIOA->AFR[0] |= (8 << (0 * 4)) | (8 << (1 * 4));

    GPIOA->PUPDR &= ~(GPIO_PUPDR_PUPD0 | GPIO_PUPDR_PUPD1);
    GPIOA->PUPDR |=  (GPIO_PUPDR_PUPD0_0 | GPIO_PUPDR_PUPD1_0);

    // 3. UART Hardware Configuration
    UART4->CR1 = 0;
    UART4->BRR = 0x0683; 

    // Enable TX and RX, but NO RXNE interrupt (DMA handles RX)
    UART4->CR1 |= (USART_CR1_TE | USART_CR1_RE);
    
    // Route incoming data signals directly to the DMA hardware
    UART4->CR3 |= USART_CR3_DMAR;  
    
    dma1_stream2_uart4_rx_init(s_rx_buffer, RX_BUFFER_SIZE);

    // Enable the Peripheral globally
    UART4->CR1 |= USART_CR1_UE;

    // 4. NVIC Enable (TX Interrupt only)
    NVIC_SetPriority(UART4_IRQn, 5); 
    NVIC_EnableIRQ(UART4_IRQn);

    return UART_OK;
}

uart_status_t uart_send(const uint8_t *p_data, uint16_t length) {
    if (p_data == NULL || length == 0) {
        return UART_ERR_PARAM;
    }

    uint16_t current_head = s_tx_head;
    uint16_t current_tail = s_tx_tail;
    uint16_t free_space = (current_tail - current_head - 1) & TX_BUFFER_MASK;

    if (length > free_space) {
        return UART_ERR_OVERFLOW;
    }

    for (uint16_t i = 0; i < length; i++) {
        s_tx_buffer[current_head] = p_data[i];
        current_head = (current_head + 1) & TX_BUFFER_MASK;
    }

    __disable_irq();
    s_tx_head = current_head;
    // Ignite Transmit Data Register Empty Hardware Interrupt
    UART4->CR1 |= USART_CR1_TXEIE;
    __enable_irq();

    return UART_OK; 
}

uart_status_t uart_read(uint8_t *p_data) {
    if (p_data == NULL) {
        return UART_ERR_PARAM;
    }

    // 1. Calculate dynamic head based on DMA's remaining data count
    uint16_t current_head = RX_BUFFER_SIZE - dma1_stream2_get_ndtr();

    // 2. If tail catches up to head, there is no new data to read
    if (s_rx_tail == current_head) {
        return UART_ERR_EMPTY;
    }

    // 3. Extract the byte
    *p_data = s_rx_buffer[s_rx_tail];

    // 4. Advance the tail safely
    s_rx_tail = (s_rx_tail + 1) & RX_BUFFER_MASK;

    return UART_OK;
}

/**
 * @brief Scans the RX buffer for new frames and posts events.
 * Call this in the main super-loop.
 */
void uart_process_rx(void)
{
    uint8_t byte;

    while (uart_read(&byte) == UART_OK)
    {
        frame_manager_process_byte(byte);
    }
}

void UART4_IRQHandler(void) {
    // 2. TX BLOCK: Only TX interrupt is enabled and handled
    if ((UART4->SR & USART_SR_TXE) && (UART4->CR1 & USART_CR1_TXEIE)) {
        if (s_tx_tail != s_tx_head) {
            UART4->DR = s_tx_buffer[s_tx_tail];
            s_tx_tail = (s_tx_tail + 1) & TX_BUFFER_MASK;
        } else {
            // Queue Empty: Disable interrupt to prevent infinite looping
            UART4->CR1 &= ~USART_CR1_TXEIE;
        }
    }
}