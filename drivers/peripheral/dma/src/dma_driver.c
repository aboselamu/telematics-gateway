#include "dma_driver.h"

void dma1_stream5_usart2_rx_init(volatile uint8_t* dest_buffer, uint16_t buffer_size ){

    //1. enable power grid for dma1 on ahb1 bus
    RCC->AHB1ENR |= (1<<21);

    //2. DIAABLE DMA1 stream 5 before config
    DMA1_Stream5->CR &= ~(1<<0);
    
    while(DMA1_Stream5->CR & (1<<0)){}

    //3. Map the silicon: P2M, from to where, 
    DMA1_Stream5->PAR = (uint32_t)&USART2->DR; // SOURCE
    DMA1_Stream5->M0AR = (uint32_t)dest_buffer; // destination

    //4.Set the Buffer size, for countdown
    DMA1_Stream5->NDTR = buffer_size;

    // 5. Configure the Stream Control Register (CR)
    // CHSEL[27:25] = 4 (Channel 4 for USART2_RX)
    // MINC = 1 (Auto-increment memory address)
    // CIRC = 1 (Circular mode: wrap to start when done)
    // PL[17:16] = 1 (Priority level: Medium)
    DMA1_Stream5->CR |= (4U << 25) | DMA_SxCR_MINC | DMA_SxCR_CIRC | (1U << 16);

    // 6. Enable the DMA Stream
    DMA1_Stream5->CR |= (1<<0);

}

uint16_t dma1_stream5_get_ndtr(void) {
    // Return the Number of Data to Transfer register for DMA1 Stream 5
    return DMA1_Stream5->NDTR; 
}