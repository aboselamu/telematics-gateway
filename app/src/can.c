#include "stm32f446xx.h"
#include "can.h"
#include <stddef.h>
#include "timebase.h"


#define CAN_TX_MAILBOX_TIMEOUT_MS 5U
#define CAN_TX_COMPLETE_TIMEOUT_MS 5U
#define CAN_RX_FIFO0_TIMEOUT_MS 5U

static void CAN1_Gpio_init(void);

static void CAN1_Gpio_init(void){
        /* GPIOB clock */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;

    /* PB8 = Alternate Function */
    GPIOB->MODER &= ~(3U << 16U);
    GPIOB->MODER |=  (2U << 16U);

    /* PB8 = AF9 = CAN1_RX */
    GPIOB->AFR[1] &= ~(0xFU << 0U);
    GPIOB->AFR[1] |=  (9U << 0U);

    /* PB8 pull-up: recessive CAN_RX */
    GPIOB->PUPDR &= ~(3U << 16U);
    GPIOB->PUPDR |=  (1U << 16U);

    /* PB9 = Alternate Function */
    GPIOB->MODER &= ~(3U << 18U);
    GPIOB->MODER |=  (2U << 18U);

    /* PB9 = AF9 = CAN1_TX */
    GPIOB->AFR[1] &= ~(0xFU << 4U);
    GPIOB->AFR[1] |=  (9U << 4U);
}

int CAN1_Init(void)
{

    /* 1. Enable CAN1 clock */
    RCC->APB1ENR |= RCC_APB1ENR_CAN1EN;

    // Egziabiher ka'ene gar nawu= AMEN
    CAN1_Gpio_init();

    /* 2. Leave sleep mode */
    CAN1->MCR &= ~CAN_MCR_SLEEP;

    /* 3. Request initialization mode */
    CAN1->MCR |= CAN_MCR_INRQ;

    /* Wait until bxCAN acknowledges INIT mode */
    while ((CAN1->MSR & CAN_MSR_INAK) == 0U)
    {

    }

    /*
    * 4. Configure 500 kbit/s
    *
    * CAN clock = 45 MHz
    *
    * BRP = 5  -> register value 4
    * BS1 = 15 -> register value 14
    * BS2 = 2  -> register value 1
    * SJW = 1  -> register value 0
    *
    * Silent + Loopback enabled
    */
    CAN1->BTR =
        (4U  << 0U)
        | (14U << 16U)
        | (1U  << 20U)
        | (0U  << 24U)
        | CAN_BTR_LBKM
        | CAN_BTR_SILM;


    /* 5. Configure filter bank 0: accept all */

    /* Enter filter initialization mode */
    CAN1->FMR |= CAN_FMR_FINIT;

    /* Disable filter bank 0 while configuring */
    CAN1->FA1R &= ~CAN_FA1R_FACT0;
    /* Mask mode: 0 = mask mode */
    CAN1->FM1R &= ~CAN_FM1R_FBM0;

    /* 32-bit scale: 1 = 32-bit */
    CAN1->FS1R |= CAN_FS1R_FSC0;

    /* Assign bank 0 to FIFO0: 0 = FIFO0 */
    CAN1->FFA1R &= ~CAN_FFA1R_FFA0;

    /* Accept everything */
    CAN1->sFilterRegister[0].FR1 = 0x00000000U;
    CAN1->sFilterRegister[0].FR2 = 0x00000000U;

    /* Activate filter bank 0 */
    CAN1->FA1R |= CAN_FA1R_FACT0;

    /* Leave filter initialization mode */
    CAN1->FMR &= ~CAN_FMR_FINIT;



    /* 6. Leave initialization mode */
    CAN1->MCR &= ~CAN_MCR_INRQ;

    /* Wait until bxCAN leaves INIT mode */
    while ((CAN1->MSR & CAN_MSR_INAK) != 0U)
    {
    }
    

    return 0;
}
// () & 
CAN_Status_t CAN1_SendPolling(const CAN_frame_t *tx_frame) // uint8_t data for only testing 
{
    if (tx_frame == NULL)
    {
        return CAN_ERROR_NULL_PTR;
    }

    if(tx_frame->dlc > 8U) return CAN_ERROR_INVALID_DLC;

    // valid: 0x000 ... 0x7FF
    if(tx_frame->id > 0x7FF ) return CAN_ERROR_INVALID_ID;
    //d 
    /* Test 3: Load TX mailbox 0 only */
    /* Check mailbox 0 is empty */
    uint32_t t_start = millis();
    while ((CAN1->TSR & CAN_TSR_TME0) == 0U)
    {
        if( millis() - t_start >= CAN_TX_MAILBOX_TIMEOUT_MS) return CAN_ERROR_TX_MAILBOX_TIMEOUT;
    }

    /* Standard ID = 0x123, IDE = 0, RTR = 0, TXRQ = 0 */
    CAN1->sTxMailBox[0].TIR = ((uint32_t)tx_frame->id << CAN_TI0R_STID_Pos);


    /* DLC = 1 */
    CAN1->sTxMailBox[0].TDTR = tx_frame->dlc;

    uint32_t tdlr = 0U;
    uint32_t tdhr = 0U;

    for (uint32_t i = 0U; i < tx_frame->dlc; i++)
    {
        if (i < 4U)
        {
            tdlr |= (uint32_t)tx_frame->data[i] << (i * 8U);
        }
        else
        {
            tdhr |= (uint32_t)tx_frame->data[i] << ((i - 4U) * 8U);
        }
    }
    CAN1->sTxMailBox[0].TDLR = tdlr;
    CAN1->sTxMailBox[0].TDHR = tdhr;

    /* Request transmission */
    CAN1->sTxMailBox[0].TIR |= CAN_TI0R_TXRQ;

    /*
    wait RQCP0
        → check TXOK0
        → clear RQCP0
        → return status
    */
    uint32_t t_start1 = millis();
    while ((CAN1->TSR & CAN_TSR_RQCP0) == 0U)
    {
        // wait for request completion:
            // TEST
        // CAN1->TSR
        // CAN1->ESR   

        if ((millis() - t_start1) >= CAN_TX_COMPLETE_TIMEOUT_MS)
        {
            return CAN_ERROR_TX_COMPLETE_TIMEOUT;
        }
    }

    //  → check TXOK0
    if( (CAN1->TSR & CAN_TSR_TXOK0) == 0U ) 
    {
        // → clear RQCP0
        // Bit 0 RQCP0: Request completed mailbox0
        CAN1->TSR = CAN_TSR_RQCP0;
        return CAN_ERROR_TX;
    }

    // → clear RQCP0
    CAN1->TSR = CAN_TSR_RQCP0;
    return CAN_OK;

}

CAN_Status_t CAN1_ReceivePolling(CAN_frame_t *rx_frame)
{
    if (rx_frame == NULL)
    {
        return CAN_ERROR_NULL_PTR;
    }
    
    uint32_t t_start2 = millis();
    while ((CAN1->RF0R & CAN_RF0R_FMP0_Msk) == 0U)
    {
        // wait until FIFO0 actually contains a frame
        if( (millis() - t_start2) >= CAN_RX_FIFO0_TIMEOUT_MS){
        return CAN_ERROR_RX_TIMEOUT;
    }
    }

    /* Read received frame */
    rx_frame->id =
        (uint16_t)((CAN1->sFIFOMailBox[0].RIR &
                    CAN_RI0R_STID_Msk)
                    >> CAN_RI0R_STID_Pos);

    rx_frame->dlc =
        (uint8_t)(CAN1->sFIFOMailBox[0].RDTR &
                CAN_RDT0R_DLC_Msk);
    
    if (rx_frame->dlc > 8U)
    {
        // Release FIFO0
        CAN1->RF0R = CAN_RF0R_RFOM0;
        return CAN_ERROR_INVALID_DLC;
    }

    
    uint32_t rx_rdlr = (uint32_t)(CAN1->sFIFOMailBox[0].RDLR);
    uint32_t rx_rdhr = (uint32_t)(CAN1->sFIFOMailBox[0].RDHR);

    for(uint32_t i = 0U; i < rx_frame->dlc; i++){

        if(i < 4U)
        {
            rx_frame->data[i] = (uint8_t) ( (rx_rdlr >> (i * 8U) ) & 0xFFU);
        }
        else
        {
            rx_frame->data[i] = (uint8_t) ( ( rx_rdhr >> ((i - 4U) * 8U) ) & 0xFFU); 
        }

    }

    /* Release FIFO0 */
    CAN1->RF0R = CAN_RF0R_RFOM0;

    return CAN_OK;

}