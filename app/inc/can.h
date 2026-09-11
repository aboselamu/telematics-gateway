#ifndef CAN_H
#define CAN_H
#include <stdint.h>

typedef struct
{
    uint16_t id;
    uint8_t  dlc;
    uint8_t  data[8];

} CAN_frame_t;

typedef enum
{
    CAN_OK = 0,

    CAN_ERROR_INVALID_DLC          = -1,
    CAN_ERROR_INVALID_ID           = -2,
    CAN_ERROR_NULL_PTR             = -3,
    CAN_ERROR_TX                   = -4,
    CAN_ERROR_TX_MAILBOX_TIMEOUT   = -5,
    CAN_ERROR_TX_COMPLETE_TIMEOUT  = -6,
    CAN_ERROR_RX_TIMEOUT           = -7

} CAN_Status_t;

int CAN1_Init(void);

CAN_Status_t CAN1_SendPolling(const CAN_frame_t *frame); // const not to modify while holding the address

CAN_Status_t CAN1_ReceivePolling(CAN_frame_t *frame);

#endif