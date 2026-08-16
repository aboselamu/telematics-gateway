#include "i2c_driver_it.h"
#include "stm32f4xx.h"
#include <stddef.h>

/*=============================================================================
 * Context Structure
 *============================================================================*/
typedef enum
{
    I2C_PHASE_IDLE = 0,
    I2C_PHASE_TX,
    I2C_PHASE_RX
} i2c_phase_t;

typedef struct
{
    uint8_t address;

    const uint8_t *p_tx_data;
    uint16_t tx_length;
    volatile uint16_t tx_index;

    uint8_t *p_rx_data;
    uint16_t rx_length;
    volatile uint16_t rx_index;

    volatile i2c_state_t state;
    volatile i2c_status_t result;
    volatile i2c_phase_t phase;

} i2c_it_context_t;

/* Static context instance, private to this file */
static i2c_it_context_t s_context;

static i2c_it_callback_t s_callback = NULL;

/*=============================================================================
 * Primitive Functions
 *============================================================================*/

static void i2c_it_generate_start(void)
{
    I2C1->CR1 |= I2C_CR1_START;
}


static void i2c_it_generate_stop(void)
{
    I2C1->CR1 |= I2C_CR1_STOP;
}


static void i2c_it_write_dr(uint8_t byte)
{
    I2C1->DR = byte;
}

static uint8_t i2c_it_read_dr(void)
{
    return (uint8_t)I2C1->DR;
}

static void i2c_it_clear_addr(void)
{
    (void)I2C1->SR1;
    (void)I2C1->SR2;
}

// cr2
static void i2c_it_enable_interrupts(void)
{
    I2C1->CR2 |= (I2C_CR2_ITEVTEN |
                  I2C_CR2_ITBUFEN |
                  I2C_CR2_ITERREN);
}

// cr2
static void i2c_it_disable_interrupts(void)
{
    I2C1->CR2 &= ~(I2C_CR2_ITEVTEN |
                   I2C_CR2_ITBUFEN |
                   I2C_CR2_ITERREN);
}


static void i2c_it_nvic_init(void)
{
    NVIC_ClearPendingIRQ(I2C1_EV_IRQn);
    NVIC_ClearPendingIRQ(I2C1_ER_IRQn);

    NVIC_EnableIRQ(I2C1_EV_IRQn);
    NVIC_EnableIRQ(I2C1_ER_IRQn);
}

// cr1
static void i2c_it_enable_ack(void)
{
    I2C1->CR1 |= I2C_CR1_ACK;
}

// cr1
static void i2c_it_disable_ack(void)
{
    I2C1->CR1 &= ~I2C_CR1_ACK;
}

// cr1
static void i2c_it_disable_pos(void)
{
    I2C1->CR1 &= ~I2C_CR1_POS;
}

// cr1
static void i2c_it_enable_pos(void)
{
    I2C1->CR1 |= I2C_CR1_POS;
}



/*=============================================================================
 * Interrupt Driver Initialisation
 *============================================================================*/

void i2c_it_init(void)
{
    i2c_it_disable_interrupts();
    i2c_it_nvic_init();

    s_context.address   = 0U;

    s_context.p_tx_data = NULL;
    s_context.tx_length = 0U;
    s_context.tx_index  = 0U;

    s_context.p_rx_data = NULL;
    s_context.rx_length = 0U;
    s_context.rx_index  = 0U;

    s_context.result = I2C_OK;
    s_context.state  = I2C_STATE_READY;
    s_context.phase  = I2C_PHASE_IDLE;
    s_callback = NULL;  
}

void i2c_it_register_callback(i2c_it_callback_t callback)
{
    s_callback = callback;
}

//================================
// helper function 
static uint8_t i2c_it_is_rx_phase(void)
{
    return ((s_context.state == I2C_STATE_BUSY_RX) ||
           ((s_context.state == I2C_STATE_BUSY_TX_RX) &&
            (s_context.phase == I2C_PHASE_RX)));
}


static void i2c_it_complete(i2c_status_t result,
                            i2c_state_t final_state)
{
    s_context.result = result;
    s_context.state  = final_state;
    s_context.phase  = I2C_PHASE_IDLE;

    if (s_callback != NULL)
    {
        s_callback(result);
    }
}

// ===============================
/*=============================================================================
 * Public Write API
 *============================================================================*/

i2c_status_t i2c_write_it(uint8_t address,
                          const uint8_t *p_data,
                          uint16_t length)
{
    /* Parameter validation */
    if ((address > 0x7FU) ||
        (p_data == NULL) ||
        (length == 0U))
    {
        return I2C_ERR_PARAM;
    }

    /* Driver state validation */
    if (s_context.state == I2C_STATE_RESET)
    {
        return I2C_ERR_NOT_INIT;
    }

    if (s_context.state == I2C_STATE_ERROR)
    {
        return I2C_ERR_LOCKED;
    }

    if (s_context.state != I2C_STATE_READY)
    {
        return I2C_ERR_BUSY;
    }

    /* Physical bus validation */
    if ((I2C1->SR2 & I2C_SR2_BUSY) != 0U)
    {
        return I2C_ERR_BUS;
    }

    /* Save persistent transaction context */
    s_context.address   = address;
    s_context.p_tx_data = p_data;
    s_context.tx_length = length;
    s_context.tx_index  = 0U;

    /* Acquire peripheral ownership */
    s_context.state = I2C_STATE_BUSY_TX;
    s_context.phase = I2C_PHASE_TX;

    /* Start asynchronous transaction */
    i2c_it_enable_interrupts();
    i2c_it_generate_start();

    /*
     * I2C_OK here means:
     * request accepted and transaction started.
     */
    return I2C_OK;
}

// read
i2c_status_t i2c_read_it(uint8_t address,
                         uint8_t *p_data,
                         uint16_t length)
{
    if ((address > 0x7FU) ||
        (p_data == NULL) ||
        (length == 0U))
    {
        return I2C_ERR_PARAM;
    }

    if (s_context.state == I2C_STATE_RESET)
        return I2C_ERR_NOT_INIT;

    if (s_context.state == I2C_STATE_ERROR)
        return I2C_ERR_LOCKED;

    if (s_context.state != I2C_STATE_READY)
        return I2C_ERR_BUSY;

    if ((I2C1->SR2 & I2C_SR2_BUSY) != 0U)
        return I2C_ERR_BUS;

    s_context.address   = address;
    s_context.p_rx_data = p_data;
    s_context.rx_length = length;
    s_context.rx_index  = 0U;

    s_context.state = I2C_STATE_BUSY_RX;
    s_context.phase = I2C_PHASE_RX;

    i2c_it_enable_ack();
    i2c_it_disable_pos();

    i2c_it_enable_interrupts();
    i2c_it_generate_start();

    return I2C_OK;
}

// write-read
i2c_status_t i2c_write_read_it(uint8_t address,
                               const uint8_t *p_tx_data,
                               uint16_t tx_length,
                               uint8_t *p_rx_data,
                               uint16_t rx_length)
{
    if ((address > 0x7FU) ||
        (p_tx_data == NULL) ||
        (p_rx_data == NULL) ||
        (tx_length == 0U) ||
        (rx_length == 0U))
    {
        return I2C_ERR_PARAM;
    }

    if (s_context.state == I2C_STATE_RESET)
        return I2C_ERR_NOT_INIT;

    if (s_context.state == I2C_STATE_ERROR)
        return I2C_ERR_LOCKED;

    if (s_context.state != I2C_STATE_READY)
        return I2C_ERR_BUSY;

    if ((I2C1->SR2 & I2C_SR2_BUSY) != 0U)
        return I2C_ERR_BUS;

    s_context.address   = address;

    s_context.p_tx_data = p_tx_data;
    s_context.tx_length = tx_length;
    s_context.tx_index  = 0U;

    s_context.p_rx_data = p_rx_data;
    s_context.rx_length = rx_length;
    s_context.rx_index  = 0U;

    s_context.state = I2C_STATE_BUSY_TX_RX;
    s_context.phase = I2C_PHASE_TX;

    i2c_it_enable_ack();
    i2c_it_disable_pos();

    i2c_it_enable_interrupts();
    i2c_it_generate_start();

    return I2C_OK;
}

i2c_state_t i2c_it_get_state(void)
{
    return s_context.state;
}

i2c_status_t i2c_it_get_result(void)
{
    return s_context.result;
}

/*=============================================================================
 * I2C1 Event Interrupt Handler
 *============================================================================*/

void I2C1_EV_IRQHandler(void)
{
    uint32_t sr1 = I2C1->SR1;

    /* 1. START generated */
    if ((sr1 & I2C_SR1_SB) != 0U)
    {
        if (s_context.phase == I2C_PHASE_TX)
        {
            /* Initial START: address + WRITE */
            i2c_it_write_dr((uint8_t)(s_context.address << 1U));
        }
        else if (s_context.phase == I2C_PHASE_RX)
        {
            /* Repeated START: address + READ */
            i2c_it_write_dr((uint8_t)((s_context.address << 1U) | 1U));
        }
        return;
    }

    /*  2. Address acknowledged */
    if ((sr1 & I2C_SR1_ADDR) != 0U)
    {
        if (i2c_it_is_rx_phase() && (s_context.rx_length == 1U))
        {
            uint32_t primask_state;

            /* Must happen BEFORE clearing ADDR */
            i2c_it_disable_ack();

            /* Need RXNE for final byte */
            I2C1->CR2 |= I2C_CR2_ITBUFEN;

            primask_state = __get_PRIMASK();
            __disable_irq();

            i2c_it_clear_addr();

            /* STOP is programmed before receiving the byte */
            i2c_it_generate_stop();

            __set_PRIMASK(primask_state);
        }
        else if(i2c_it_is_rx_phase()  && (s_context.rx_length == 2U))
        {
            /*
            * Special STM32 2-byte receive sequence.
            * Configure POS/ACK before clearing ADDR.
            */
            i2c_it_enable_pos();
            i2c_it_disable_ack();

            /*
            * Do NOT service RXNE individually.
            * We want BTF when both bytes are available.
            */
            I2C1->CR2 &= ~I2C_CR2_ITBUFEN;

            i2c_it_clear_addr();
        }
        else if (i2c_it_is_rx_phase() && (s_context.rx_length > 2U))
        {
            if (s_context.rx_length == 3U) // last 3 bytes and it will just disable
            {
                I2C1->CR2 &= ~I2C_CR2_ITBUFEN;
            }
            else
            {
                I2C1->CR2 |= I2C_CR2_ITBUFEN;
            }

            i2c_it_clear_addr();
        }
            
        else
        {
            i2c_it_clear_addr();
        }
        return;
    }

    /* 3. BTF, Final transmitted byte completed, before txe */
    if ((sr1 & I2C_SR1_BTF) != 0U)
    {
        /* Standalone WRITE completion */
        if ((s_context.state == I2C_STATE_BUSY_TX) &&
            (s_context.tx_index == s_context.tx_length))
        {
            i2c_it_generate_stop();

            i2c_it_disable_interrupts();

            i2c_it_complete(I2C_OK, I2C_STATE_READY);

            return;
        } 

        /* Combined WRITE → READ */
        else if ((s_context.state == I2C_STATE_BUSY_TX_RX) && (s_context.phase == I2C_PHASE_TX) 
                 && (s_context.tx_index == s_context.tx_length))
        {   
            /*
            * NO STOP.
            * Change protocol phase and request repeated START.
            */
            s_context.phase = I2C_PHASE_RX;

            i2c_it_generate_start();
            
            // Otherwise an old TXE flag from the same snapshot 
            // could be processed after I already changed to RX
            return;
        }

         // 2-byte READ completion 
        else if (i2c_it_is_rx_phase() && (s_context.rx_length == 2U))
        {
            uint32_t primask_state;

            primask_state = __get_PRIMASK();
            __disable_irq();

            i2c_it_generate_stop();

            s_context.p_rx_data[0] = i2c_it_read_dr();

            __set_PRIMASK(primask_state);

            s_context.p_rx_data[1] = i2c_it_read_dr();

            s_context.rx_index = 2U;

            i2c_it_disable_interrupts();

            i2c_it_enable_ack();
            i2c_it_disable_pos();

            i2c_it_complete(I2C_OK, I2C_STATE_READY);

            return;
        }

        else if ( i2c_it_is_rx_phase() && (s_context.rx_length > 2U))
        {
            uint16_t remaining = s_context.rx_length - s_context.rx_index;

            /* First final-3-byte handshake */
            if (remaining == 3U)
            {
                i2c_it_disable_ack();

                /* Read N-2 */
                s_context.p_rx_data[s_context.rx_index] = i2c_it_read_dr();

                s_context.rx_index++;

                return;
            }
            
            /* Second handshake */
            else if (remaining == 2U)
            {
                uint32_t primask_state;

                primask_state = __get_PRIMASK();
                __disable_irq();

                i2c_it_generate_stop();

                /* Read N-1 */
                s_context.p_rx_data[s_context.rx_index] = i2c_it_read_dr();

                __set_PRIMASK(primask_state);

                s_context.rx_index++;

                /*
                * One final byte remains.
                * Re-enable RXNE interrupt so it can finish the transfer.
                */
                I2C1->CR2 |= I2C_CR2_ITBUFEN;

                return;
            }
        }
    }

    /* 4. TXE, DR ready for next payload byte */
    if ((sr1 & I2C_SR1_TXE) != 0U)
    {
        if (((s_context.state == I2C_STATE_BUSY_TX) || ((s_context.state == I2C_STATE_BUSY_TX_RX) &&
      (s_context.phase == I2C_PHASE_TX))) && (s_context.tx_index < s_context.tx_length))
        {
            i2c_it_write_dr(
                s_context.p_tx_data[s_context.tx_index]);

            s_context.tx_index++;

            /*
             * All bytes have now been loaded.
             * Stop TXE interrupts and wait for final BTF.
             */
            if (s_context.tx_index == s_context.tx_length)
            {
                I2C1->CR2 &= ~I2C_CR2_ITBUFEN;
            }
        }
        return;
    }

    // 5. RXNE, DR READY for next byte to receive, rx
    if ((sr1 & I2C_SR1_RXNE) != 0U)
    {
        if (i2c_it_is_rx_phase() &&
            (s_context.rx_length == 1U))
        {
            s_context.p_rx_data[0] = i2c_it_read_dr();
            s_context.rx_index = 1U;

            i2c_it_disable_interrupts();

            /* Restore normal idle receive configuration */
            i2c_it_enable_ack();
            i2c_it_disable_pos();

            i2c_it_complete(I2C_OK, I2C_STATE_READY);
            
            return;
        }
        
        else if (i2c_it_is_rx_phase() && (s_context.rx_length > 2U))
        {
            uint16_t remaining = s_context.rx_length - s_context.rx_index;

            if (remaining > 3U)
            {
                s_context.p_rx_data[s_context.rx_index] = i2c_it_read_dr(); // read

                s_context.rx_index++;

                /* Exactly 3 bytes now remain:
                stop RXNE interrupts and wait for BTF. */
                if ((s_context.rx_length -
                    s_context.rx_index) == 3U)
                {
                    I2C1->CR2 &= ~I2C_CR2_ITBUFEN;
                }
            }

            /* Final byte after STOP */
            else if (remaining == 1U)
            {
                s_context.p_rx_data[s_context.rx_index] =
                    i2c_it_read_dr();

                s_context.rx_index++;

                i2c_it_disable_interrupts();

                i2c_it_enable_ack();
                i2c_it_disable_pos();

                i2c_it_complete(I2C_OK, I2C_STATE_READY);

                return;
            }
        }
    }

}

/*=============================================================================
 * I2C1 Error Interrupt Handler
    BERR  → clear/ignore because of F446 erratum
    ARLO  → hard failure, bus ownership lost, no generate stop
    AF    → expected/recoverable NACK
    OVR   → hard peripheral sequencing failure
 *============================================================================*/
void I2C1_ER_IRQHandler(void)
{
    uint32_t sr1 = I2C1->SR1;

    /*
     * STM32F446 erratum:
     * BERR may be spurious in controller/master mode.
     * Clear it and continue processing the transaction.
     */
    if ((sr1 & I2C_SR1_BERR) != 0U)
    {
        I2C1->SR1 &= ~I2C_SR1_BERR;

        /* Remove it from our local snapshot as well. */
        sr1 &= ~I2C_SR1_BERR;
    }

    /* Arbitration lost: hard failure */
    if ((sr1 & I2C_SR1_ARLO) != 0U)
    {
        I2C1->SR1 &= ~I2C_SR1_ARLO;

        /*
         * Do not generate STOP here.
         * Arbitration loss means this controller no longer owns the bus.
         */
        i2c_it_disable_interrupts();

        i2c_it_complete(I2C_ERR_ARBITRATION_LOST, I2C_STATE_ERROR);

        return;
    }

    /* NACK: recoverable transaction failure */
    if ((sr1 & I2C_SR1_AF) != 0U)
    {
        I2C1->SR1 &= ~I2C_SR1_AF;

        i2c_it_generate_stop();
        i2c_it_disable_interrupts();

        i2c_it_complete(I2C_ERR_NACK, I2C_STATE_READY);

        return;
    }

    /* Overrun / underrun: hard failure */
    if ((sr1 & I2C_SR1_OVR) != 0U)
    {
        I2C1->SR1 &= ~I2C_SR1_OVR;

        i2c_it_generate_stop();
        i2c_it_disable_interrupts();

        i2c_it_complete(I2C_ERR_OVERRUN, I2C_STATE_ERROR);  

        return;
    }
}
 


/* === NOTES====.
 uint8_t address;
 const uint8_t *p_tx_data;
 uint16_t tx_length;

 They are loaded before the transaction starts and then only read by the ISR. 
 tx_index, state, and result are the changing shared fields.

 Do not create state merely because state machines are fashionable. 
 Add state when the existing information is insufficient or becomes difficult to reason about.

 The short answer is: Because an Interrupt Service Routine (ISR) cannot take parameters, 
 And the original address variable no longer exists by the time the interrupt fires.

 ============================================
 write transaction:
    address + W
    payload bytes

read transaction:
    address + R

write-read transaction:
    address + W
    register/subaddress bytes
    repeated START
    address + R
 ============================================
 I2C peripheral
    |
    | SR1 flags become active
    v
CR2 interrupt enables
    |
    v
NVIC
    |
    +--> I2C1_EV_IRQHandler()
    |
    +--> I2C1_ER_IRQHandler()
 ============================================
 */

 