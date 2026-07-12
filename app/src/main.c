#include "uart_driver.h"
#include "event_queue.h"
#include "nmea_parser.h"
#include "stm32f4xx.h"  
// #include <stddef.h>

// USART1 (PC / FT232RL) - 115200 Baud
// ==========================================
void USART1_Init(void) {
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;

    GPIOA->MODER &= ~((3 << (9 * 2)) | (3 << (10 * 2)));
    GPIOA->MODER |=  ((2 << (9 * 2)) | (2 << (10 * 2)));

    GPIOA->AFR[1] &= ~((0xF << ((9 - 8) * 4)) | (0xF << ((10 - 8) * 4)));
    GPIOA->AFR[1] |=  ((7 << ((9 - 8) * 4))   | (7 << ((10 - 8) * 4)));

    USART1->BRR = 0x8A; 
    USART1->CR1 |= (USART_CR1_UE | USART_CR1_TE | USART_CR1_RE);
}

void USART1_Write_Raw(char ch) {
    while (!(USART1->SR & USART_SR_TXE)); 
    USART1->DR = ch;
}

void USART1_Write_String(const char *str)
{
    while (*str)
    {
        USART1_Write_Raw(*str++);
    }
}

// ==========================================
// Main Execution
// ==========================================
int main(void) {
    USART1_Init(); // PC

    // Print a startup message so we know the STM32 is alive
    USART1_Write_String("\r\nDMA Ready\r\n");

    eventQueue_init(); 
    uart_config_t cfg = { .baud_rate = 9600 }; 
    uart_init(&cfg); 
    while (1) 
    { 
        uart_process_rx(); // check for me if a data is arrived on rx pin to rx buffer side
        event_t evt; 
        if (eventQueue_poll(&evt) == EVENT_QUEUE_OK){
            
            if (evt.event_id == EVT_UART_FRAME_READY){

                const char *frame = uart_get_frame();

                switch (nmea_parse(frame))
                {
                    case NMEA_GPRMC:
                        USART1_Write_String("GPRMC\r\n");
                        USART1_Write_String(frame);
                        break;

                    case NMEA_GPGGA:
                        USART1_Write_String("GPGGA\r\n");
                        USART1_Write_String(frame);
                        break;

                    case NMEA_GPGSV:
                        USART1_Write_String("GPGSV\r\n");
                        USART1_Write_String(frame);
                        break;

                    case NMEA_GPGSA:
                        USART1_Write_String("GPGSA\r\n");
                        USART1_Write_String(frame);
                        break;

                    case NMEA_GPVTG:
                        USART1_Write_String("GPVTG\r\n");
                        USART1_Write_String(frame);
                        break;

                    case NMEA_GPGLL:
                        USART1_Write_String("GPGLL\r\n");
                        USART1_Write_String(frame);
                        break;

                    default:
                        USART1_Write_String("UNKNOWN\r\n");
                        USART1_Write_String(frame);
                        break;
                }
           } 

        } 
    } 
}