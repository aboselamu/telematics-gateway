#include "i2c_driver.h"
#include "stm32f4xx.h"
#include <stddef.h>

/*=============================================================================
 * Private Definitions
 *============================================================================*/

#define I2C1_SCL_PIN          6U
#define I2C1_SDA_PIN          7U

#define I2C1_AF               4U
#define GPIO_AF_BITS          4U

#define APB1_CLOCK_HZ         45000000UL

#define MHZ_DIVISOR           1000000UL

// #define I2C_STANDARD_MODE     100000UL   


#define I2C_FAST_MODE         400000UL
#define I2C_OAR1_BIT14_MUST_BE_ONE    (1UL << 14U)

//For my V1, this is just a software loop counter. Later, I can replace it with 
// a timer-based timeout without changing the rest of the driver, like watchdog.
/*
 * V1 polling timeout expressed as a raw iteration count.
 *
 * The real timeout duration depends on CPU frequency, compiler
 * optimisation and peripheral-register access latency. At 180 MHz,
 * it is substantially shorter than at the 16 MHz HSI configuration.
 *
 * Unexpected I2C_ERR_TIMEOUT results should therefore be checked
 * against this implementation before assuming a physical bus fault.
 *
 * Replace with a hardware timer or system-tick deadline in a later
 * driver revision.
 */
#define I2C_TIMEOUT_COUNT    100000UL

static i2c_config_t s_config;
static uint8_t s_config_valid = 0U;

/*=============================================================================
 * Driver Context (V1)
 *============================================================================*/

static i2c_state_t s_state = I2C_STATE_RESET;

/*=============================================================================
 * Private Function Prototypes, , WORKERS, Direct above layer from registers/silcon itself
 *============================================================================*/

static i2c_status_t i2c_configure_timing(uint32_t clock_speed);
static i2c_status_t i2c_generate_start(void);
static i2c_status_t i2c_send_address(uint8_t address, uint8_t read);
static i2c_status_t i2c_send_byte(uint8_t data);
//
static inline void i2c_generate_stop(void);
static i2c_status_t i2c_wait_sr1_flag(uint32_t flag);
static void i2c_clear_error_flags(uint32_t sr1);
static void i2c_terminate_bus_request(void);
// static i2c_status_t i2c_abort_transaction(i2c_status_t status);
static i2c_status_t i2c_receive_byte(uint8_t *p_data);
static void i2c_clear_addr_flag(void);
static i2c_status_t i2c_receive_1_byte(uint8_t address, uint8_t *p_data);
static i2c_status_t i2c_receive_2_bytes(uint8_t address, uint8_t *p_data);
static i2c_status_t i2c_receive_n_bytes(uint8_t address, uint8_t *p_data, uint16_t length);
static inline void i2c_enable_ack(void);
static inline void i2c_disable_ack(void);
static inline void i2c_enable_pos(void);
static inline void i2c_disable_pos(void);
static void i2c_read_dr(uint8_t *p_data);
static i2c_status_t i2c_abort_sequence(i2c_status_t error_status);
static i2c_status_t i2c_complete_transaction(void);// wait for stop

static i2c_status_t i2c_transmit_phase(uint8_t address,
                                       const uint8_t *p_data,
                                       uint16_t length);
static i2c_status_t i2c_reset_and_configure_peripheral(const i2c_config_t *p_config);

/*=============================================================================
 * Public Functions, PUBLIC APIs, MANAGERS, organize transaction.
 *============================================================================*/

i2c_status_t i2c_init(const i2c_config_t *p_config)
{
    i2c_status_t status;

    if (p_config == NULL)
    {
        return I2C_ERR_PARAM;
    }
    s_config_valid = 0U;
    s_state = I2C_STATE_RESET;

    /*----------------------------------------------------------
     * Enable Peripheral Clocks
     *---------------------------------------------------------*/

    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;

    /*
    * Read back the clock-enable registers to ensure the writes
    * have reached the RCC before accessing the peripherals.
    */
    (void)RCC->AHB1ENR;
    (void)RCC->APB1ENR;


    /*----------------------------------------------------------
     * Configure GPIO
     *---------------------------------------------------------*/

    /* Clear and set MODER safely in one clock instruction */
    GPIOB->MODER = (GPIOB->MODER & ~((3U << (I2C1_SCL_PIN * 2U)) | (3U << (I2C1_SDA_PIN * 2U)))) 

                                 |  ((2U << (I2C1_SCL_PIN * 2U)) | (2U << (I2C1_SDA_PIN * 2U)));

    /* Open Drain */

    GPIOB->OTYPER |=
        (1U << I2C1_SCL_PIN) |
        (1U << I2C1_SDA_PIN);

    /* Very High Speed */

    GPIOB->OSPEEDR &= ~(
        (3U << (I2C1_SCL_PIN * 2U)) |
        (3U << (I2C1_SDA_PIN * 2U)));

    GPIOB->OSPEEDR |= (
        (3U << (I2C1_SCL_PIN * 2U)) |
        (3U << (I2C1_SDA_PIN * 2U)));

    /* External pull-ups are used */

    GPIOB->PUPDR &= ~(
        (3U << (I2C1_SCL_PIN * 2U)) |
        (3U << (I2C1_SDA_PIN * 2U)));

    /* Alternate Function AF4 */

    GPIOB->AFR[0] &= ~(
        (0xFU << (I2C1_SCL_PIN * GPIO_AF_BITS)) |
        (0xFU << (I2C1_SDA_PIN * GPIO_AF_BITS)));

    GPIOB->AFR[0] |= (
        (I2C1_AF << (I2C1_SCL_PIN * GPIO_AF_BITS)) |
        (I2C1_AF << (I2C1_SDA_PIN * GPIO_AF_BITS)));

    /*----------------------------------------------------------
     * Configure Timing
     *---------------------------------------------------------*/

    status = i2c_reset_and_configure_peripheral(p_config);

    if (status != I2C_OK)
    {
        return status;
    }

     /*
     * Save only a validated configuration that has been
     * successfully applied to the hardware.
     */
    s_config = *p_config;
    s_config_valid = 1U;
    s_state = I2C_STATE_READY;

    return I2C_OK;
}

// read
i2c_status_t i2c_read(uint8_t address, uint8_t *p_data, uint16_t length)
{
    i2c_status_t status;

    /* 1. Parameter Validation */
    if (p_data == NULL || length == 0U || address > 0x7FU)
    {
        return I2C_ERR_PARAM;
    }

    /* 2. State Gatekeeping */
    switch (s_state)
    {
        case I2C_STATE_READY:
            /*
            * Software is available, but verify that the physical
            * bus is also idle before requesting an initial START.
            */
            if ((I2C1->SR2 & I2C_SR2_BUSY) != 0U)
            {
                /*
                * In this V1 single-controller design, BUSY while the
                * driver says READY indicates an inconsistent or stuck
                * bus condition.
                */
                s_state = I2C_STATE_ERROR;
                return I2C_ERR_BUS;
            }

            s_state = I2C_STATE_BUSY_RX;
            break;

        case I2C_STATE_ERROR:
            return I2C_ERR_LOCKED;

        case I2C_STATE_RESET:
            return I2C_ERR_NOT_INIT;

        case I2C_STATE_BUSY_RX:
        case I2C_STATE_BUSY_TX:
        case I2C_STATE_BUSY_TX_RX:
            return I2C_ERR_BUSY;

        default:
            return I2C_ERR_LOCKED; 
    }

    /* 3. Dispatch to Specific RM0390 Hardware Sequence Helpers */
    if (length == 1U)
    {
        status = i2c_receive_1_byte(address, p_data);
    }
    else if (length == 2U)
    {
        status = i2c_receive_2_bytes(address, p_data);
    }
    else
    {
        status = i2c_receive_n_bytes(address, p_data, length);
    }

    /* 4. Strict State Lifecycle Enforcement */
    if (status == I2C_OK)
    {
        /*
        * Normal successful transaction:
        * verify STOP completion and restore idle configuration.
        */
        status = i2c_complete_transaction();

        if (status == I2C_OK)
        {
            s_state = I2C_STATE_READY;
        }
        else
        {
            s_state = I2C_STATE_ERROR;
        }
    }
    else if (status == I2C_ERR_NACK)
    {
        i2c_status_t cleanup_status;

        /*
        * The sequence abort function has already requested STOP.
        * Complete the bus teardown before allowing another transaction.
        */
        cleanup_status = i2c_complete_transaction();

        if (cleanup_status == I2C_OK)
        {
            s_state = I2C_STATE_READY;

            /* Preserve the original transaction result. */
            return I2C_ERR_NACK;
        }

        /*
        * NACK recovery itself failed, so the hardware state
        * can no longer be trusted.
        */
        status = cleanup_status;
        s_state = I2C_STATE_ERROR;
    }
    else
    {
        /*
        * Timeout, bus error, overrun, or arbitration-related
        * failures remain locked until proper recovery.
        */
        s_state = I2C_STATE_ERROR;
    }

    return status;
}

// write
i2c_status_t i2c_write(uint8_t address,
                       const uint8_t *p_data,
                       uint16_t length)
{
    i2c_status_t status;

    /*----------------------------------------------------------
     * 1. Parameter validation
     *---------------------------------------------------------*/

    if ((p_data == NULL) ||
        (length == 0U)   ||
        (address > 0x7FU))
    {
        return I2C_ERR_PARAM;
    }

    /*----------------------------------------------------------
     * 2. Acquire driver ownership
     *
     * V1 contract:
     * - Polling
     * - Single execution context
     * - Not reentrant
     * - Must not be called from an ISR
     *---------------------------------------------------------*/

    switch (s_state)
    {
        case I2C_STATE_READY:

            if ((I2C1->SR2 & I2C_SR2_BUSY) != 0U)
            {
                s_state = I2C_STATE_ERROR;
                return I2C_ERR_BUS;
            }

            s_state = I2C_STATE_BUSY_TX;
            break;

        case I2C_STATE_RESET:
            return I2C_ERR_NOT_INIT;

        case I2C_STATE_BUSY_RX:
        case I2C_STATE_BUSY_TX:
        case I2C_STATE_BUSY_TX_RX:
            return I2C_ERR_BUSY;

        case I2C_STATE_ERROR:
            return I2C_ERR_LOCKED;

        default:
            return I2C_ERR_LOCKED;
    }

    /*----------------------------------------------------------
     * 3. Execute the controller-transmitter sequence
     *---------------------------------------------------------*/

    status = i2c_transmit_phase(address, p_data, length);

    if (status == I2C_OK)
    {
        /*
        * Pure write transaction: terminate after final BTF.
        */
        i2c_generate_stop();
    }
    /*----------------------------------------------------------
     * 4. Successful hardware completion
     *---------------------------------------------------------*/

    if (status == I2C_OK)
    {
        /*
         * The sequence helper has requested STOP after BTF.
         * Verify that STOP completed before releasing the
         * software state.
         */
        status = i2c_complete_transaction();

        if (status == I2C_OK)
        {
            s_state = I2C_STATE_READY;
        }
        else
        {
            s_state = I2C_STATE_ERROR;
        }

        return status;
    }

    /*----------------------------------------------------------
     * 5. Recoverable transaction failure: target NACK
     *---------------------------------------------------------*/

    if (status == I2C_ERR_NACK)
    {
        i2c_status_t cleanup_status;

        /*
         * i2c_abort_sequence() has already requested STOP
         * when this peripheral still owned the bus.
         */
        cleanup_status = i2c_complete_transaction();

        if (cleanup_status == I2C_OK)
        {
            /*
             * The controller is healthy and reusable.
             * Preserve the original transaction result.
             */
            s_state = I2C_STATE_READY;
            return I2C_ERR_NACK;
        }

        /*
         * The NACK itself was recoverable, but bus teardown
         * failed. Report the cleanup failure because the
         * hardware state can no longer be trusted.
         */
        s_state = I2C_STATE_ERROR;
        return cleanup_status;
    }

    /*----------------------------------------------------------
     * 6. Unrecoverable or currently unclassified failure
     *---------------------------------------------------------*/

    s_state = I2C_STATE_ERROR;

    return status;
}

//write read
i2c_status_t i2c_write_read(
    uint8_t address,
    const uint8_t *p_tx_data,
    uint16_t tx_length,
    uint8_t *p_rx_data,
    uint16_t rx_length)
{
    i2c_status_t status;

    /*----------------------------------------------------------
     * 1. Parameter validation
     *---------------------------------------------------------*/

    if ((p_tx_data == NULL) ||
        (p_rx_data == NULL) ||
        (tx_length == 0U)   ||
        (rx_length == 0U)   ||
        (address > 0x7FU))
    {
        return I2C_ERR_PARAM;
    }

    /*----------------------------------------------------------
     * 2. Acquire the complete combined transaction
     *---------------------------------------------------------*/

    switch (s_state)
    {
        case I2C_STATE_READY:

            if ((I2C1->SR2 & I2C_SR2_BUSY) != 0U)
            {
                s_state = I2C_STATE_ERROR;
                return I2C_ERR_BUS;
            }

            s_state = I2C_STATE_BUSY_TX_RX;
            break;

        case I2C_STATE_RESET:
            return I2C_ERR_NOT_INIT;

        case I2C_STATE_BUSY_RX:
        case I2C_STATE_BUSY_TX:
        case I2C_STATE_BUSY_TX_RX:
            return I2C_ERR_BUSY;

        case I2C_STATE_ERROR:
            return I2C_ERR_LOCKED;

        default:
            return I2C_ERR_LOCKED;
    }

    /*----------------------------------------------------------
     * 3. Write phase
     *
     * The helper returns after final BTF without STOP.
     *---------------------------------------------------------*/

    status = i2c_transmit_phase(
        address,
        p_tx_data,
        tx_length);

    if (status == I2C_OK)
    {
        /*
         * The bus is still owned by I2C1.
         *
         * Each existing receive helper begins by setting START.
         * Because MSL/BUSY remain asserted, that START becomes
         * the required repeated START.
         */

        if (rx_length == 1U)
        {
            status = i2c_receive_1_byte(
                address,
                p_rx_data);
        }
        else if (rx_length == 2U)
        {
            status = i2c_receive_2_bytes(
                address,
                p_rx_data);
        }
        else
        {
            status = i2c_receive_n_bytes(
                address,
                p_rx_data,
                rx_length);
        }
    }

    /*----------------------------------------------------------
     * 4. Successful transaction completion
     *---------------------------------------------------------*/

    if (status == I2C_OK)
    {
        /*
         * The selected receive helper has already requested
         * STOP at the correct length-specific point.
         */
        status = i2c_complete_transaction();

        if (status == I2C_OK)
        {
            s_state = I2C_STATE_READY;
        }
        else
        {
            s_state = I2C_STATE_ERROR;
        }

        return status;
    }

    /*----------------------------------------------------------
     * 5. Recoverable target NACK
     *
     * NACK can occur during:
     * - Write address
     * - Write payload
     * - Read address after repeated START
     *---------------------------------------------------------*/

    if (status == I2C_ERR_NACK)
    {
        i2c_status_t cleanup_status;

        /*
         * i2c_abort_sequence() has already requested STOP
         * if this controller still owns the bus.
         */
        cleanup_status = i2c_complete_transaction();

        if (cleanup_status == I2C_OK)
        {
            s_state = I2C_STATE_READY;

            /* Preserve the original transaction result. */
            return I2C_ERR_NACK;
        }

        /*
         * The transaction NACK was recoverable, but the
         * hardware teardown failed.
         */
        s_state = I2C_STATE_ERROR;
        return cleanup_status;
    }

    /*----------------------------------------------------------
     * 6. Uncertain hardware state
     *---------------------------------------------------------*/

    s_state = I2C_STATE_ERROR;

    return status;
}

// this is publick function to clear state
i2c_status_t i2c_recover(void)
{
    i2c_status_t status;

    /*
     * Recovery requires a configuration from a previously
     * successful initialization.
     */
    if ((s_state == I2C_STATE_RESET) ||
        (s_config_valid == 0U))
    {
        return I2C_ERR_NOT_INIT;
    }

    /*
     * Never reset the peripheral underneath an active
     * transaction.
     */
    if ((s_state == I2C_STATE_BUSY_RX) || 
        (s_state == I2C_STATE_BUSY_TX) ||(s_state == I2C_STATE_BUSY_TX_RX))
    {
        return I2C_ERR_BUSY;
    }
    //   an already healthy driver requires no recovery.
    if (s_state == I2C_STATE_READY)
    {
        return I2C_OK;
    }

    /*
     * Only ERROR reaches the actual recovery operation.
     * Keep the state as ERROR throughout recovery so no
     * transaction can be admitted prematurely.
     */
    if (s_state != I2C_STATE_ERROR)
    {
        return I2C_ERR_LOCKED;
    }

    /*
     * Clocks and GPIO remain configured from i2c_init().
     * Reset and reconfigure only the I2C1 peripheral.
     */
    status = i2c_reset_and_configure_peripheral(&s_config);

    if (status != I2C_OK)
    {
        s_state = I2C_STATE_ERROR;
        return status;
    }

    /*
     * Peripheral recovery succeeded, but the physical bus
     * must also be available before the driver becomes READY.
     *
     * In this V1 single-controller design, persistent BUSY
     * indicates that SDA or SCL may still be held low.
     */
    if ((I2C1->SR2 & I2C_SR2_BUSY) != 0U)
    {
        s_state = I2C_STATE_ERROR;
        return I2C_ERR_BUS;
    }

    s_state = I2C_STATE_READY;

    return I2C_OK;
}

// optional to get the state of the driver
i2c_state_t i2c_get_state(void)
{
    return s_state;
}

//======================================================================
// transaction level
static i2c_status_t i2c_transmit_phase(uint8_t address,
                                       const uint8_t *p_data,
                                       uint16_t length)
{
    i2c_status_t status;
    uint16_t index;

    /*
     * Preconditions:
     * - Parameters have been validated by i2c_write().
     * - Driver state is I2C_STATE_BUSY_TX.
     * - Peripheral has been initialized.
     */

    /*----------------------------------------------------------
     * 1. Generate START and wait for SB
     *---------------------------------------------------------*/

    status = i2c_generate_start();

    if (status != I2C_OK)
    {
        return i2c_abort_sequence(status);
    }

    /*----------------------------------------------------------
     * 2. Send the 7-bit address in write direction
     *---------------------------------------------------------*/

    status = i2c_send_address(address, 0U);

    if (status != I2C_OK)
    {
        return i2c_abort_sequence(status);
    }

    /*----------------------------------------------------------
     * 3. Clear ADDR
     *
     * Reading SR1 followed by SR2 releases the peripheral
     * into controller-transmitter data operation.
     *---------------------------------------------------------*/

    i2c_clear_addr_flag();

    /*----------------------------------------------------------
     * 4. Transmit all requested bytes
     *
     * i2c_send_byte():
     * - waits for TXE
     * - checks transaction errors
     * - writes the byte into DR
     *
     * TXE permits pipelining: the next byte may be written
     * once DR has moved into the shift register.
     *---------------------------------------------------------*/

    for (index = 0U; index < length; index++)
    {
        status = i2c_send_byte(p_data[index]);

        if (status != I2C_OK)
        {
            return i2c_abort_sequence(status);
        }
    }

    /*----------------------------------------------------------
     * 5. Wait for the final byte to complete
     *
     * TXE alone is insufficient here. It only proves that DR
     * is empty; the final byte may still be shifting on SDA.
     *
     * BTF proves both:
     * - DR is empty
     * - the shift register has completed the byte transfer
     *
     * Waiting here also allows AF from a NACK on the final
     * data byte to be detected before STOP is requested.
     *---------------------------------------------------------*/

    status = i2c_wait_sr1_flag(I2C_SR1_BTF);

    if (status != I2C_OK)
    {
        return i2c_abort_sequence(status);
    }

    /*----------------------------------------------------------
     * 6. Request STOP at a valid byte boundary
     *---------------------------------------------------------*/

    //i2c_generate_stop(); // this should be handled by caller

    /*
     * STOP completion is intentionally not polled here.
     * The public manager performs common transaction
     * completion before changing BUSY_TX to READY.
     */

    return I2C_OK;
}

// for 1-BYTE RECEIVE 
static i2c_status_t i2c_receive_1_byte(uint8_t address, uint8_t *p_data)
{
    i2c_status_t status;
    uint32_t primask_state;

    /* 1. Force a known starting configuration */
    i2c_enable_ack();
    i2c_disable_pos();

    /* 2. START Condition */
    status = i2c_generate_start();
    if (status != I2C_OK) 
    {
        return i2c_abort_sequence(status);
    }

    /* 3. Send Address (Read Mode) */
    status = i2c_send_address(address, 1U);
    if (status != I2C_OK) 
    {
        return i2c_abort_sequence(status);
    }

    /*----------------------------------------------------------
     * 4. The Critical 1-Byte RM0390 Sequence
     *---------------------------------------------------------*/
    
    /* Disable ACK BEFORE clearing ADDR to force NACK on the 1st byte */
    i2c_disable_ack();     

    /* ========================================== */
    /*        SAFE CRITICAL SECTION               */
    /* ========================================== */
    primask_state = __get_PRIMASK();
    __disable_irq();

    /* Release the hardware to clock in the byte */
    i2c_clear_addr_flag(); 

    /* Program STOP immediately so it asserts after the byte arrives */
    i2c_generate_stop();   

    /* Restore interrupts to their exact previous state */
    __set_PRIMASK(primask_state);
    /* ========================================== */

    /*----------------------------------------------------------
     * 5. Data Retrieval
     *---------------------------------------------------------*/
    status = i2c_receive_byte(p_data);
    if (status != I2C_OK) 
    {
        return i2c_abort_sequence(status);
    }

    return I2C_OK;
}

// 2 byte refere project not under this func description 
static i2c_status_t i2c_receive_2_bytes(uint8_t address, uint8_t *p_data)
{
    i2c_status_t status;

    /* Establish a known receive configuration. */
    i2c_enable_ack();
    i2c_disable_pos();

    /* Generate START and wait for SB. */
    status = i2c_generate_start();
    if (status != I2C_OK)
    {
        return i2c_abort_sequence(status);
    }

    /* Send 7-bit target address with read direction; wait for ADDR. */
    status = i2c_send_address(address, 1U);
    if (status != I2C_OK)
    {
        return i2c_abort_sequence(status);
    }

    /*
     * ADDR is now set and SCL is held low.
     * Configure the special two-byte receive behaviour
     * before clearing ADDR.
     */

    /* 2. THE FIX: Configure future hardware behavior while clock is paused */
    i2c_enable_pos();   /* CR1.POS = 1 (NACK applies to the NEXT byte in shift register) */
    i2c_disable_ack();  /* CR1.ACK = 0 */


    /* 3. Release the clock by clearing ADDR. Hardware immediately shifts in 2 bytes. */
    i2c_clear_addr_flag();
    /* 
     * 4. Wait for BTF
     * 
     * At BTF = 1:
     *   SCL is stretched low.
     *   DR             = byte 1
     *   Shift register = byte 2
     *---------------------------------------------------------*/
    status = i2c_wait_sr1_flag(I2C_SR1_BTF);
    if (status != I2C_OK)
    {
        return i2c_abort_sequence(status);
    }

    i2c_generate_stop();
    /*----------------------------------------------------------
     * 6. Extract Data (Blind read, no waiting)
     BTF Guarantees (The hardware promises it's there): BTF stands for Byte Transfer Finished. 
     When this hardware flag turns on (1), 
     the internal silicon is telling me: 
     "The current data transfer is 100% complete and safely stored in the register
         BTF guarantees both received bytes are available:
            DR             = byte 1
            Shift register = byte 2
     *---------------------------------------------------------*/

    p_data[0] = (uint8_t)I2C1->DR;
    p_data[1] = (uint8_t)I2C1->DR;

    return I2C_OK;
}

// for n byte
static i2c_status_t i2c_receive_n_bytes(uint8_t address, uint8_t *p_data, uint16_t length)
{
    i2c_status_t status;

    /* 1. Baseline Verification: Ensure standard NACK/POS state */
    i2c_disable_pos();  /* CR1.POS = 0 */
    i2c_enable_ack();   /* CR1.ACK = 1 */

    /* 2. Initiate Bus Transaction */
    status = i2c_generate_start();
    if (status != I2C_OK) return i2c_abort_sequence(status);

    /* 3. Transmit Slave Address with READ bit (LSB = 1) */
    status = i2c_send_address(address, 1U);
    if (status != I2C_OK) return i2c_abort_sequence(status);

    /* 4. Release SCL to begin clocking data from the slave */
    i2c_clear_addr_flag();

    /* 5. Bulk Phase: Poll RXNE and read DR until 3 bytes remain */
    while (length > 3U)
    {
        status = i2c_receive_byte(p_data); /* Helper: wait RXNE -> read DR */
        if (status != I2C_OK) return i2c_abort_sequence(status);
        
        p_data++;
        length--;
    }

    /* =====================================================================
     * 6. RM0390 Strict Teardown Phase for Final 3 Bytes
     * ===================================================================== */

    /* 
     * Handshake 1: Wait for BTF. 
     * Byte N-2 is in DR, Byte N-1 is in the Shift Register. SCL is stretched LOW.
     */
    status = i2c_wait_sr1_flag(I2C_SR1_BTF);
    if (status != I2C_OK) return i2c_abort_sequence(status);

    /* Disable ACK so the hardware will NACK Byte N */
    i2c_disable_ack();

    /* Read Byte N-2. This clears BTF, releases SCL, and Byte N starts shifting in */
    i2c_read_dr(p_data);
    p_data++;

    /* 
     * Handshake 2: Wait for BTF again.
     * Byte N-1 is in DR, Byte N is in the Shift Register. SCL is stretched LOW again.
     */
    status = i2c_wait_sr1_flag(I2C_SR1_BTF);
    if (status != I2C_OK) return i2c_abort_sequence(status);

    /* Program STOP bit while SCL is stretched. STOP fires after Byte N finishes */
    i2c_generate_stop();

    /* Read Byte N-1. This clears BTF and releases SCL for the STOP condition */
    i2c_read_dr(p_data);
    p_data++;

    /* 
     * Final Byte Extraction: Byte N drops into DR.
     * We use i2c_receive_byte() here to cleanly wait for RXNE and read the final byte.
     */
    status = i2c_receive_byte(p_data);
    if (status != I2C_OK) return i2c_abort_sequence(status);

    return I2C_OK;
}

/*=============================================================================
 * Private Functions, Primitve funcs can be changed if we change the mcu.
 *============================================================================*/

//configuragion
static i2c_status_t i2c_reset_and_configure_peripheral(
    const i2c_config_t *p_config)
{
    i2c_status_t status;

    if (p_config == NULL)
    {
        return I2C_ERR_PARAM;
    }

    /*
     * Reset only the I2C1 peripheral.
     *
     * The APB1 peripheral clock must already be enabled before
     * this function is called.
     */
     
    RCC->APB1RSTR |= RCC_APB1RSTR_I2C1RST;
    (void)RCC->APB1RSTR;

    RCC->APB1RSTR &= ~RCC_APB1RSTR_I2C1RST;
    (void)RCC->APB1RSTR;

    /*
     * Configure timing while the peripheral is disabled.
     */
    I2C1->CR1 &= ~I2C_CR1_PE;

    /*
     * RM0390 requires OAR1 bit 14 to remain set.
     * The controller-only driver does not otherwise use OAR1.
     */
    I2C1->OAR1 = I2C_OAR1_BIT14_MUST_BE_ONE;

    status = i2c_configure_timing(p_config->clock_speed);

    if (status != I2C_OK)
    {
        return status;
    }

    /*
     * Enable I2C1 before establishing the software-defined
     * idle receive baseline.
     */
    I2C1->CR1 |= I2C_CR1_PE;

    i2c_enable_ack();
    i2c_disable_pos();

    return I2C_OK;
}

 // read
 static void i2c_read_dr(uint8_t *p_data)
{
    *p_data = (uint8_t)I2C1->DR;
}

static i2c_status_t i2c_configure_timing(uint32_t clock_speed)
{
    uint32_t pclk_mhz;
    uint32_t ccr;

    pclk_mhz = APB1_CLOCK_HZ / MHZ_DIVISOR;

    switch (clock_speed)
    {
        case I2C_CLOCK_STANDARD_SAFE_HZ:

            /* Configure APB1 frequency (MHz) */

            I2C1->CR2 &= ~I2C_CR2_FREQ;
            I2C1->CR2 |= pclk_mhz;

            /*
             * Standard mode:
             *
             * Fscl = PCLK1 / (2 × CCR)
             *
             * Use ceiling division so the actual SCL frequency
             * never exceeds the requested frequency. a/b =( a + b-1) /b
             */
            ccr = (APB1_CLOCK_HZ + (2UL * clock_speed) - 1UL) / (2UL * clock_speed);

            if (ccr < 4UL)
            {
                ccr = 4UL;
            }

            I2C1->CCR = ccr;

            /* Maximum Rise Time */

            I2C1->TRISE = pclk_mhz + 1U;

            break;

        case I2C_FAST_MODE:

            /* Fast Mode V2 */

            return I2C_ERR_PARAM;

        default:

            return I2C_ERR_PARAM;
    }

    return I2C_OK;
}

static i2c_status_t  i2c_generate_start(void)
{
    /* Generate START condition */
     //Bit 8 START: Start generation - bit is set and cleared by software and 
     // cleared by hardware when start is sent or PE=0
    I2C1->CR1 |= I2C_CR1_START;

    /* Wait until START condition generated */

    return i2c_wait_sr1_flag(I2C_SR1_SB);
}

static i2c_status_t i2c_send_address(uint8_t address, uint8_t read)
{
    uint8_t addr;

    // if (address > 0x7FU)
    // {
    //     return I2C_ERR_PARAM;
    // }

    addr = (uint8_t)(address << 1U);

    if (read)
    {
        addr |= 1U;
    }
    else
    {
        addr &= ~1U;
    }

    I2C1->DR = addr;

    /*
     * Worker ONLY waits for ADDR.
     * The transaction manager is responsible for clearing ADDR
     * according to the transaction sequence.
     */
    return i2c_wait_sr1_flag(I2C_SR1_ADDR);
}

// [PRIMITIVE FUNC]:- Direct write to outpt data register, 
// i utilizes helpper func i2c_wait_sr1_flag()- to check TRANSMIT DATA REGISTER EMPTY FLAG
// 
static i2c_status_t i2c_send_byte(uint8_t data)
{
    i2c_status_t status;

    status = i2c_wait_sr1_flag(I2C_SR1_TXE);

    if (status != I2C_OK)
    {
        return status; // means it could be any error, nack, bus error, ovr, so on
    }

    I2C1->DR = data;

    return I2C_OK;
}

/*
    It's only job is to flip the stop bit, 
    Remember pre-conditions for:-
        Write:- After WRITING all BYTES => WAIT for BTF = 1, THEN CALL THIS FUNC
        READ:- FOLLOW THE SEQUENCE FOR 1-BYTE, 2-BYTE and N>2-BYTE
        YOU MUST HANDLE THE LOGIC BEFORE CALLING THIS FUNCTION
    I2C1->CR1 |= I2C_CR1_STOP => executes instantly in the CPU 
    NO polling, looping, or waiting involved in that specific line of code. 
    Because there is no possibility of a timeout or an error during that assignment, 
    returning is not needed.
*/
static inline void i2c_generate_stop(void)
{
    
    I2C1->CR1 |= I2C_CR1_STOP;

}

// wait for generate stop to complete
static i2c_status_t i2c_complete_transaction(void)
{
    uint32_t timeout = I2C_TIMEOUT_COUNT;
    uint32_t sr2;

    while ((I2C1->CR1 & I2C_CR1_STOP) != 0U)
    {
        if (timeout == 0U)
        {
            return I2C_ERR_TIMEOUT;
        }

        timeout--;
    }

    sr2 = I2C1->SR2;

    /*
     * V1 is a single-controller implementation.
     * After STOP completion, this peripheral must no longer
     * be controller and the physical bus must be idle.
     */
    if ((sr2 & (I2C_SR2_MSL | I2C_SR2_BUSY)) != 0U)
    {
        return I2C_ERR_BUS;
    }

    i2c_enable_ack();
    i2c_disable_pos();

    return I2C_OK;
}

// primitive fun, for waiting for rxne, txe,sb... refer project descripion section 
static i2c_status_t i2c_wait_sr1_flag(uint32_t flag)
{
    uint32_t timeout = I2C_TIMEOUT_COUNT;

    while (timeout > 0U)
    {
        uint32_t sr1 = I2C1->SR1;

        /*
         * STM32F446 erratum:
         * BERR may be spurious in controller mode.
         * Clear it and continue processing the transaction.
         */
        if ((sr1 & I2C_SR1_BERR) != 0U)
        {
            i2c_clear_error_flags(I2C_SR1_BERR);

            //STM32F446 erratum: in controller mode, a spurious BERR should be cleared
            //while the transfer continues.
            sr1 &= ~I2C_SR1_BERR;
        }

        /* Genuine transaction errors take priority over success. */
        if ((sr1 & I2C_SR1_ARLO) != 0U)
        {
            i2c_clear_error_flags(I2C_SR1_ARLO);
            return I2C_ERR_ARBITRATION_LOST;
        }

        if ((sr1 & I2C_SR1_AF) != 0U)
        {
            i2c_clear_error_flags(I2C_SR1_AF);
            return I2C_ERR_NACK;
        }

        if ((sr1 & I2C_SR1_OVR) != 0U)
        {
            i2c_clear_error_flags(I2C_SR1_OVR);
            return I2C_ERR_OVERRUN;
        }

        if ((sr1 & flag) == flag)
        {
            return I2C_OK;
        }

        timeout--;
    }

    return I2C_ERR_TIMEOUT;
}

// clear SR1 ERRORS ONLY
static void i2c_clear_error_flags(uint32_t sr1)
{
    /* 
     * STM32F446RE I2C_SR1 rc_w0 error flag bits built strictly from CMSIS symbols:
     * - Bit 8:  I2C_SR1_BERR
     * - Bit 9:  I2C_SR1_ARLO
     * - Bit 10: I2C_SR1_AF
     * - Bit 11: I2C_SR1_OVR
     * - Bit 12: I2C_SR1_PECERR
     * - Bit 14: I2C_SR1_TIMEOUT
     * - Bit 15: I2C_SR1_SMBALERT
     * (Excludes reserved Bit 13 and includes BERR at Bit 8).
     */
    const uint32_t I2C_SR1_RCW0_MASK = (I2C_SR1_BERR    |
                                       I2C_SR1_ARLO    |
                                       I2C_SR1_AF      |
                                       I2C_SR1_OVR     |
                                       I2C_SR1_PECERR  |
                                       I2C_SR1_TIMEOUT |
                                       I2C_SR1_SMBALERT);

    /* Isolate only the specific errors captured in the snapshot */
    uint32_t errors_to_clear = sr1 & (I2C_SR1_BERR | I2C_SR1_ARLO | I2C_SR1_AF | I2C_SR1_OVR);

    /*
     * 1. Start with all rc_w0 error bits set to 1 (preventing them from clearing).
     * 2. Clear the specific error bits we want to wipe (writing 0 clears them).
     * 3. Reserved and unselected bits remain untouched safely.
     */
    I2C1->SR1 = I2C_SR1_RCW0_MASK & ~errors_to_clear;
}

static i2c_status_t i2c_abort_sequence(i2c_status_t error_status)
{
    /*
     * Terminate or cancel the current hardware request.
     * Driver-state handling remains the manager's responsibility.
     */
    i2c_terminate_bus_request();

    return error_status;
}

static void i2c_terminate_bus_request(void)
{
    if ((I2C1->SR2 & I2C_SR2_MSL) != 0U)
    {
        
        //This peripheral currently owns the bus.
        //Request STOP to terminate the transaction.
        
        i2c_generate_stop();
    }
    else
    {
        // The peripheral never became master, or ownership
        // was lost. Cancel any outstanding START request.
        I2C1->CR1 &= ~I2C_CR1_START;
    }
}

// primitive func to grab 1 byte from the input data register
static i2c_status_t i2c_receive_byte(uint8_t *p_data)
{
    i2c_status_t status;

    if (p_data == NULL)
    {
        return I2C_ERR_PARAM;
    }

    status = i2c_wait_sr1_flag(I2C_SR1_RXNE);

    if (status != I2C_OK)
    {
        return status;
    }

    *p_data = (uint8_t)I2C1->DR;

    return I2C_OK;
}

static void i2c_clear_addr_flag(void)
{
    /*
      Clear ADDR by reading SR1 followed by SR2.
      Required by RM0390 (EV6).
     */
    (void)I2C1->SR1;
    (void)I2C1->SR2;
}

/* Bit 10 ACK: Acknowledge enable
   Enables automatic ACK generation after each received byte.
   used for Multi-BYTE I2C REVEPTION, TO HELP MASTER TO ACK 
   EACH RECIEVED BYTE UNTIL FINAL BYTE SEQU
   Enables the I2C peripheral to automatically return an ACK after receiving a byte.
*/
static inline void i2c_enable_ack(void)
{
    I2C1->CR1 |= I2C_CR1_ACK; //CR1.ACK = 1
}

/*
    By disabling ACK, it forces the I2C peripheral to return a NACK after the next byte.
    CALL This Func:- right before the final byte of a read transaction to tell the slave to stop.
*/
static inline void i2c_disable_ack(void) 
{
    I2C1->CR1 &= ~I2C_CR1_ACK; //I2C->CR1 |= 0 << 10, rm0390-stm32f446xx
}

/* ENABLES ACK POSITION CONTROL, FOR 2-BYTE RECEIVE SEQU
   WHEN POS = 1, the NACK, GENERATED BY CLEARING ACK, 
   WILL ONLY APPLIES TO THE SECOND RECIEVED BYTE 
   i.e:- when POS= 1 for 2-BYTE read
        1). Byte 1 to be ACKed (so the slave sends Byte)
        2). Byte 2 to be NACKed (so the slave stops)
*/
static inline void i2c_enable_pos(void)
{
    I2C1->CR1 |= I2C_CR1_POS; // 1 << 11, rm0390-stm32f446xx
}

// Disables ACK position control (restores default behavior).
// Always clear POS at the end of a 2-BYTE transaction.
static inline void i2c_disable_pos(void)
{
    I2C1->CR1 &= ~I2C_CR1_POS; // I2C-CR1 |= 0<<11
}

