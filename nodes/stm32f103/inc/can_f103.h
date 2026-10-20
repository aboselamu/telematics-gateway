#ifndef CAN_F103_H
#define CAN_F103_H

#include <stdint.h>

/* STM32F103C8T6 CAN1, Classic CAN, standard 11-bit DATA frames only.
 * v1 bring-up uses polling TX/RX and SILENT LOOPBACK (no CAN bus traffic).
 */
typedef struct
{
    uint16_t id;      /* 0x000..0x7FF */
    uint8_t  dlc;     /* 0..8 */
    uint8_t  data[8];
} can_f103_frame_t;

typedef enum
{
    CAN_F103_OK = 0,
    CAN_F103_ERR_INIT_TIMEOUT = -1,
    CAN_F103_ERR_EXIT_INIT_TIMEOUT = -2,
    CAN_F103_ERR_PARAM = -3,
    CAN_F103_ERR_TX_MAILBOX_TIMEOUT = -4,
    CAN_F103_ERR_TX_COMPLETE_TIMEOUT = -5,
    CAN_F103_ERR_TX_FAILED = -6,
    CAN_F103_ERR_RX_TIMEOUT = -7,
    CAN_F103_ERR_RX_BAD_DLC = -8
} can_f103_status_t;

/* Requires PCLK1 = 36 MHz (verified clock step). */
can_f103_status_t CAN_F103_InitSilentLoopback(void);
can_f103_status_t CAN_F103_InitNormal(void);
can_f103_status_t CAN_F103_SendPolling(const can_f103_frame_t *frame);
can_f103_status_t CAN_F103_ReceivePolling(can_f103_frame_t *frame);

#endif /* CAN_F103_H */
