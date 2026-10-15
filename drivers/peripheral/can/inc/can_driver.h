#ifndef CAN_DRIVER_H
#define CAN_DRIVER_H

#include <stdint.h>

/* Phase 5 / Step 1: STM32F446RE CAN1, standard 11-bit data frames. */
typedef struct
{
    uint16_t id;          /* 0x000 - 0x7FF */
    uint8_t dlc;          /* 0 - 8 */
    uint8_t data[8];
} CAN_frame_t;

typedef enum
{
    CAN_OK                          = 0,
    CAN_ERROR_INVALID_DLC           = -1,
    CAN_ERROR_INVALID_ID            = -2,
    CAN_ERROR_NULL_PTR              = -3,
    CAN_ERROR_TX                    = -4,
    CAN_ERROR_TX_MAILBOX_TIMEOUT    = -5,
    CAN_ERROR_TX_COMPLETE_TIMEOUT   = -6,
    CAN_ERROR_RX_TIMEOUT            = -7, /* reserved for future polling RX */
    CAN_ERROR_RX_EMPTY              = -8,
    CAN_ERROR_TX_BUSY               = -9,
    CAN_ERROR_INIT_TIMEOUT          = -10
} CAN_Status_t;

typedef enum
{
    CAN_MODE_SILENT_LOOPBACK = 0,
    CAN_MODE_NORMAL
} CAN_Mode_t;

/* Compatibility entry point: same behaviour as standalone CAN1_Init(). */
int CAN1_Init(void);

/* Future physical-bus tests can select CAN_MODE_NORMAL explicitly. */
CAN_Status_t CAN1_InitMode(CAN_Mode_t mode);

/* First integration milestone: polling TX and IRQ-fed RX software queue. */
CAN_Status_t CAN1_SendPolling(const CAN_frame_t *frame);
CAN_Status_t CAN1_Read(CAN_frame_t *frame);

/* Diagnostic: count frames dropped because the software RX queue was full. */
uint32_t CAN1_RxDropCount(void);

#endif /* CAN_DRIVER_H */
