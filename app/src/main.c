#include "uart_driver.h"
#include "event_queue.h"
#include "nmea_parser.h"
#include "frame_manager.h"
#include "gps_decoder.h"
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
int main(void)
{
    USART1_Init();   // PC Debug UART

    USART1_Write_String("\r\nDMA Ready\r\n");
    
    const char test_frame[] = "$GPRMC,131341.00,A,2612.28124,S,02800.48003,E,0.985,12.5,250626,,,A*6C";

    gps_data_t gps;

    if (gps_decode_rmc(test_frame, &gps))
    {
        USART1_Write_String("\r\n===== TEST PARSE =====\r\n");

        USART1_Write_String("UTC: ");
        USART1_Write_String(gps.utc_time);
        USART1_Write_String("\r\n");

        USART1_Write_String("LAT: ");
        USART1_Write_String(gps.latitude);
        USART1_Write_Raw(' ');
        USART1_Write_Raw(gps.latitude_dir);
        USART1_Write_String("\r\n");

        USART1_Write_String("LON: ");
        USART1_Write_String(gps.longitude);
        USART1_Write_Raw(' ');
        USART1_Write_Raw(gps.longitude_dir);
        USART1_Write_String("\r\n");

        USART1_Write_String("SPD: ");
        USART1_Write_String(gps.speed);
        USART1_Write_String("\r\n");

        USART1_Write_String("FIX: ");
        USART1_Write_String(gps.valid_fix ? "VALID" : "INVALID");
        USART1_Write_String("\r\n");
    }

    eventQueue_init();
    frame_manager_init();

    uart_config_t cfg =
    {
        .baud_rate = 9600
    };

    uart_init(&cfg);

    while (1)
{
    uart_process_rx();

    event_t evt;

    if (eventQueue_poll(&evt) == EVENT_QUEUE_OK)
    {
        if (evt.event_id == EVT_UART_FRAME_READY)
        {
            frame_t *frame = (frame_t *)evt.param1;
            gps_data_t gps;

            switch (nmea_parse((const char *)frame->data))
            {
                case NMEA_GPRMC:

                    if (gps_decode_rmc((const char *)frame->data, &gps))
                    {
                        USART1_Write_String("\r\n===== GPRMC =====\r\n");

                        USART1_Write_String("UTC: ");
                        USART1_Write_String(gps.utc_time);
                        USART1_Write_String("\r\n");

                        USART1_Write_String("LAT: ");
                        USART1_Write_String(gps.latitude);
                        USART1_Write_Raw(' ');
                        USART1_Write_Raw(gps.latitude_dir);
                        USART1_Write_String("\r\n");

                        USART1_Write_String("LON: ");
                        USART1_Write_String(gps.longitude);
                        USART1_Write_Raw(' ');
                        USART1_Write_Raw(gps.longitude_dir);
                        USART1_Write_String("\r\n");

                        USART1_Write_String("SPD: ");
                        USART1_Write_String(gps.speed);
                        USART1_Write_String("\r\n");

                        USART1_Write_String("FIX: ");
                        USART1_Write_String(gps.valid_fix ? "VALID" : "INVALID");
                        USART1_Write_String("\r\n");
                    }

                    break;

                default:
                    break;
            }

            /* Return the frame to the pool */
            frame_manager_release((int8_t)evt.param2);
        }
    }
}
}