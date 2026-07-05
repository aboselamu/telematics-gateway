#ifndef DMA_DRIVER_H
#define DMA_DRIVER_H

#include "stm32f4xx.h"
#include <stdint.h>

// Initializes DMA1 Stream 5 for peripheral-to-memory transfer (Targeted for USART2 RX).
void dma1_stream5_usart2_rx_init(volatile uint8_t * dest_buffer, uint16_t buffer_size);


// get ndtr 
uint16_t dma1_stream5_get_ndtr(void);

#endif // DMA_DRIVER_H
