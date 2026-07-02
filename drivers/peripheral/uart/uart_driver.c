/**
 * @file uart_driver.c
 * @brief Private implementation of the non-blocking UART driver.
 * * Implements a dual circular ring buffer architecture with decoupled cursors,
 * atomic state synchronization, and a claim-ticket notification framework.
 */

#include "uart_driver.h"
#include "event_queue.h"
#include "stm32f4xx.h"  // Core CMSIS and Peripheral Register Definitions
#include <stddef.h>


/* Buffer Sizes must be a power of two for efficient bitwise wrapping mask */
#define RX_BUFFER_SIZE 256
#define TX_BUFFER_SIZE 256

#define RX_BUFFER_MASK (RX_BUFFER_SIZE - 1)
#define TX_BUFFER_MASK (TX_BUFFER_SIZE - 1)

/* Private Internal Driver State Storage */
static uint8_t  s_rx_buffer[RX_BUFFER_SIZE];
static volatile uint16_t s_rx_head = 0;
static volatile uint16_t s_rx_tail = 0;

static uint8_t  s_tx_buffer[TX_BUFFER_SIZE];
static volatile uint16_t s_tx_head = 0;
static volatile uint16_t s_tx_tail = 0;

uart_status_t uart_init(const uart_config_t *p_config) {
    if (p_config == NULL) {
        return UART_ERR_PARAM;
    }

    // Reset internal state indexes
    s_rx_head = 0;   
    s_rx_tail = 0;   
    s_tx_head = 0;   
    s_tx_tail = 0;   

    // 1. Clock Configuration (Targeting USART2 on APB1, GPIOA on AHB1)
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

    // 2. GPIO Pin Configuration (PA2 = TX, PA3 = RX)
    // Clear mode bits for Pin 2 and Pin 3 cleanly
    GPIOA->MODER &= ~(GPIO_MODER_MODER2 | GPIO_MODER_MODER3);
    // Set both to Alternate Function mode (0b10)
    GPIOA->MODER |= (GPIO_MODER_MODER2_1 | GPIO_MODER_MODER3_1);

    // Completely clear AF slots for BOTH Pin 2 and Pin 3 (4 bits per pin slot)
    GPIOA->AFR[0] &= ~((0xF << (2 * 4)) | (0xF << (3 * 4))); 
    // Map both pins to AF7 (USART2)
    GPIOA->AFR[0] |= (7 << (2 * 4)) | (7 << (3 * 4));

    // 3. USART Hardware Configuration
    // Clear Control Register 1 to establish a baseline state
    USART2->CR1 = 0;

    // Calculate Baud Rate (Assumes maxed 180MHz system clock -> PCLK1 = 45MHz)
    // For 115200: 45000000 / (16 * 115200) = 24.414 -> Mantissa 24 (0x18), Fraction 7 (0x7)
    if (p_config->baud_rate == 115200) {
        USART2->BRR = 0x0187; 
    } else {
        // Safe default configuration fallback
        USART2->BRR = 0x0187; 
    }

    // Configure Control Register 1: Enable TX, RX, and RXNE Interrupt
    USART2->CR1 |= (USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE);

    // Enable the Peripheral globally
    USART2->CR1 |= USART_CR1_UE;

    // 4. Nested Vectored Interrupt Controller (NVIC) Enable
    NVIC_SetPriority(USART2_IRQn, 5); 
    NVIC_EnableIRQ(USART2_IRQn);

    return UART_OK;
}

uart_status_t uart_send(const uint8_t *p_data, uint16_t length) {

    // CHECK IF the message is not empty / validate the data
    if (p_data == NULL || length == 0) {
        return UART_ERR_PARAM;
    }

    // Calculate free space in the TX ring buffer/ 
    uint16_t current_head = s_tx_head; //  local variable to add the char to internal buffer, s_tx_buffer
    uint16_t current_tail = s_tx_tail;
    uint16_t free_space = (current_tail - current_head - 1) & TX_BUFFER_MASK;

    if (length > free_space) {
        return UART_ERR_OVERFLOW; // Not enough space to store the entire chunk atomically
    }

    // Copy data package into the internal transmit queue
    for (uint16_t i = 0; i < length; i++) {
        s_tx_buffer[current_head] = p_data[i];
        current_head = (current_head + 1) & TX_BUFFER_MASK;
    }

    
    // Atomic update of the shared head index
    __disable_irq();
    s_tx_head = current_head;
    
    // Ignite Transmit Data Register Empty Hardware Interrupt
    USART2->CR1 |= USART_CR1_TXEIE;
    __enable_irq();

    return UART_OK; 
}

uint16_t uart_read(uint8_t *p_dest, uint16_t max_len, uint16_t claim_ticket) {
    uint16_t bytes_read = 0;

    // Safety Boundary Check: Abort if queue is completely empty
    if (s_rx_tail == s_rx_head) {
        return 0;
    }

    // Decoupled Cursor: Traverse buffer using a localized variable
    // Ensures the ISR boundaries aren't chasing a rapidly oscillating target
    uint16_t read_cursor = s_rx_tail;

    // Leave exactly 1 byte free at the end of max_len for secure null-termination
    while (bytes_read < (max_len - 1)) {
        
        p_dest[bytes_read] = s_rx_buffer[read_cursor];
        bytes_read++;

        // Check if the decoupled cursor hit the precise ticket boundary
        if (read_cursor == claim_ticket) {
            break;
        }

        read_cursor = (read_cursor + 1) & RX_BUFFER_MASK;
    }

    // Append null-termination to secure string manipulation tasks in app layer
    p_dest[bytes_read] = '\0';

    // The Critical Section Flush
    // Whether completed cleanly or truncated early due to max_len constraints,
    // the hardware tail is fast-forwarded atomically to release used slots instantly.
    __disable_irq();
    s_rx_tail = (claim_ticket + 1) & RX_BUFFER_MASK;
    __enable_irq();

    return bytes_read;
}

void USART2_IRQHandler(void) {
    
    // ==========================================
    // 1. RX BLOCK: Evaluates Data Arrival & Gate State
    // ==========================================
    if ((USART2->SR & USART_SR_RXNE) && (USART2->CR1 & USART_CR1_RXNEIE)) {
        
        uint8_t received_char = USART2->DR; // Reading DR register drops the RXNE flag
        uint16_t next_head = (s_rx_head + 1) & RX_BUFFER_MASK;

        // Collision Check protection: Verify against the current physical tail position
        if (next_head != s_rx_tail) {
            
            s_rx_buffer[s_rx_head] = received_char;
          //p_dest[bytes_read] = s_rx_buffer[read_cursor];
            // Frame Boundary Evaluation: Dispatches ticket payload upon identifying delimiter
            if (received_char == '\n') {
                event_t rx_event={0};   // zeroes all fields first
                rx_event.event_id = EVT_UART_FRAME_READY;
                rx_event.param1   = s_rx_head; // Assign frame index bookmark to envelope
                
                eventQueue_post(&rx_event);
            }

            s_rx_head = next_head; //7
            
        } else {
            // Buffer Overrun Error Handling
            // Hardware dropped character to explicitly guarantee historical data retention
        }
    }

    // ==========================================
    // 2. TX BLOCK: Evaluates Register Empty State & Active Intent
    // ==========================================
 
    if ((USART2->SR & USART_SR_TXE) && (USART2->CR1 & USART_CR1_TXEIE)) {   // it will check until all char is sent
        
        if (s_tx_tail != s_tx_head) {
            // Queue Contains Data: Stream payload directly to transmission register
            USART2->DR = s_tx_buffer[s_tx_tail];
            s_tx_tail = (s_tx_tail + 1) & TX_BUFFER_MASK;
        } else {
            // Queue Empty: Terminate interrupt signals to block infinite loop execution path
            USART2->CR1 &= ~USART_CR1_TXEIE;
        }
    }
}