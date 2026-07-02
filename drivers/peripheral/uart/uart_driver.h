/**
 * @file uart_driver.h
 * @brief Public interface for the non-blocking, event-driven UART driver.
 * * This file defines the configuration structures and APIs required to interface
 * with the UART hardware without exposing internal register states or buffer mechanics.
 */

#pragma once
#include <stdint.h>

/**
 * @brief Driver status enumeration for API return values.
 */
typedef enum {
    UART_OK = 0,
    UART_ERR_PARAM,
    UART_ERR_BUSY,
    UART_ERR_OVERFLOW
} uart_status_t;

/**
 * @brief Configuration structure for UART initialization.
 */
typedef struct {
    uint32_t baud_rate;
    // Additional parameters like parity, stop bits can be expanded here
} uart_config_t;

/**
 * @brief Initializes the UART hardware peripheral and internal driver state.
 * @param p_config Pointer to the initialization configuration structure.
 * @return uart_status_t Status of the operation.
 */
uart_status_t uart_init(const uart_config_t *p_config);

/**
 * @brief Transmits data asynchronously by placing it into the internal ring buffer.
 * Ignites the TXE interrupt mechanism to handle transmission in the background.
 * @param p_data Pointer to the byte array to transmit.
 * @param length Number of bytes to transmit.
 * @return uart_status_t UART_OK if successful, UART_ERR_OVERFLOW if transmission queue is full.
 */
uart_status_t uart_send(const uint8_t *p_data, uint16_t length);

/**
 * @brief Extracts a complete frame from the RX ring buffer up to the claim ticket marker.
 * Defensively handles buffer boundaries and isolates the main loop from the ISR.
 * @param p_dest Pointer to the application buffer where extracted data will be written.
 * @param max_len Maximum capacity of the destination buffer (including room for null-terminator).
 * @param claim_ticket The hardware ring buffer index corresponding to the end of the frame ('\n').
 * @return uint16_t The exact number of bytes successfully extracted and written to p_dest.
 */
uint16_t uart_read(uint8_t *p_dest, uint16_t max_len, uint16_t claim_ticket);

// #endif /* UART_DRIVER_H_ */