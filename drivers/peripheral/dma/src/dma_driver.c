/**
 * @file dma_driver.c
 * @brief DMA Peripheral configuration for non-blocking UART4 communication.
 */

#include "dma_driver.h"
#include "stm32f4xx.h" 

void dma1_stream2_uart4_rx_init(volatile uint8_t* dest_buffer, uint16_t buffer_size) {

    // 1. Enable power grid for DMA1 on AHB1 bus
    // RCC_AHB1ENR_DMA1EN is bit 21
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;

    // 2. DISABLE DMA1 Stream 2 before configuration
    DMA1_Stream2->CR &= ~DMA_SxCR_EN;
    
    // Wait for the stream to disable (safety check)
    while (DMA1_Stream2->CR & DMA_SxCR_EN) {}

    // 3. Map the silicon: Peripheral to Memory
    DMA1_Stream2->PAR  = (uint32_t)&(UART4->DR);   // SOURCE: UART4 Data Register
    DMA1_Stream2->M0AR = (uint32_t)dest_buffer;    // DESTINATION: Ring buffer address

    // 4. Set the Buffer size (NDTR for countdown)
    DMA1_Stream2->NDTR = buffer_size;

    // 5. Configure the Stream Control Register (CR)
    // CHSEL[27:25] = 4 (Channel 4 for UART4_RX)
    // MINC = 1 (Auto-increment memory address)
    // CIRC = 1 (Circular mode: wrap to start when done)
    // PL[17:16] = 1 (Priority level: Medium)
    DMA1_Stream2->CR &= ~(DMA_SxCR_CHSEL | DMA_SxCR_PL); // Clear Channel and Priority
    DMA1_Stream2->CR |= (4U << 25) | DMA_SxCR_MINC | DMA_SxCR_CIRC | (1U << 16);

    // 6. Enable the DMA Stream
    DMA1_Stream2->CR |= DMA_SxCR_EN;
}

uint16_t dma1_stream2_get_ndtr(void) {
    // Return the Number of Data to Transfer register for DMA1 Stream 2
    return DMA1_Stream2->NDTR; 
}