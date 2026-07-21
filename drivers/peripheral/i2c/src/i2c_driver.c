#include "i2c_driver.h"
#include "stm32f4xx.h"

/*=============================================================================
 * Private Definitions
 *============================================================================*/

#define I2C1_SCL_PIN          6U
#define I2C1_SDA_PIN          7U

#define I2C1_AF               4U
#define GPIO_AF_BITS          4U

#define APB1_CLOCK_HZ         45000000UL

#define MHZ_DIVISOR           1000000UL

#define I2C_STANDARD_MODE     100000UL
#define I2C_FAST_MODE         400000UL

//For my V1, this is just a software loop counter. Later, I can replace it with 
// a timer-based timeout without changing the rest of the driver, like watchdog.
#define I2C_TIMEOUT_COUNT    100000UL


/*=============================================================================
 * Driver Context (V1)
 *============================================================================*/

static i2c_state_t s_state = I2C_STATE_RESET;

/*=============================================================================
 * Private Function Prototypes
 *============================================================================*/

static i2c_status_t i2c_configure_timing(uint32_t clock_speed);
static i2c_status_t i2c_generate_start(void);
static i2c_status_t i2c_send_address(uint8_t address, uint8_t read);
static i2c_status_t i2c_send_byte(uint8_t data);
static i2c_status_t i2c_generate_stop(void);
static i2c_status_t i2c_wait_sr1_flag(uint32_t flag);
static void i2c_clear_error_flags(uint32_t sr1);
static void i2c_reset_bus(void);

/*=============================================================================
 * Public Functions
 *============================================================================*/

i2c_status_t i2c_init(const i2c_config_t *p_config)
{
    i2c_status_t status;

    if (p_config == NULL)
    {
        return I2C_ERR_PARAM;
    }

    s_state = I2C_STATE_RESET;

    /*----------------------------------------------------------
     * Enable Peripheral Clocks
     *---------------------------------------------------------*/

    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;

    /* CRITICAL DELAY: Read back the register to stall CPU until clock stabilizes */
    __IO uint32_t tmp_reg = RCC->APB1ENR;
    (void)tmp_reg;

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

    status = i2c_configure_timing(p_config->clock_speed);

    if (status != I2C_OK)
    {
        return status;
    }

    /*----------------------------------------------------------
     * Enable Peripheral
     *---------------------------------------------------------*/

    I2C1->CR1 |= I2C_CR1_PE;

    s_state = I2C_STATE_READY;

    return I2C_OK;
}

/*=============================================================================
 * Private Functions
 *============================================================================*/

static i2c_status_t i2c_configure_timing(uint32_t clock_speed)
{
    uint32_t pclk_mhz;

    pclk_mhz = APB1_CLOCK_HZ / MHZ_DIVISOR;

    switch (clock_speed)
    {
        case I2C_STANDARD_MODE:

            /* Configure APB1 frequency (MHz) */

            I2C1->CR2 &= ~I2C_CR2_FREQ;
            I2C1->CR2 |= pclk_mhz;

            /* Standard Mode
             * CCR = PCLK / (2 × Fscl)
             */

            I2C1->CCR =
                APB1_CLOCK_HZ /
                (2UL * I2C_STANDARD_MODE);

            /* Maximum Rise Time */

            I2C1->TRISE =
                pclk_mhz + 1U;

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
    assert(address <= 0x7F);
    uint8_t addr = address << 1U;

    if (read)
    {
        addr |= 1U;
    }
    else
    {
        addr &= ~1U;
    }

    I2C1->DR = addr;
    
    i2c_status_t status;
    status = i2c_wait_sr1_flag(I2C_SR1_ADDR);

    if (status != I2C_OK)
    {
        return status;
    }
    /*
     * The final evaluation of the polling loop has already
     * performed the required SR1 read. Reading SR2 completes
     * the sequence that clears the ADDR flag.
     */
    (void)I2C1->SR2;
    
    return I2C_OK;
}

static i2c_status_t i2c_send_byte(uint8_t data)
{
    i2c_status_t status;

    status = i2c_wait_sr1_flag(I2C_SR1_TXE);

    if (status != I2C_OK)
    {
        return status;
    }

    I2C1->DR = data;

    return I2C_OK;
}

static i2c_status_t i2c_generate_stop(void)
{
    i2c_status_t status;

    status = i2c_wait_sr1_flag(I2C_SR1_BTF);

    if (status != I2C_OK)
    {
        return status;
    }

    I2C1->CR1 |= I2C_CR1_STOP;

    return I2C_OK;
}

static i2c_status_t i2c_wait_sr1_flag(uint32_t flag)
{
    uint32_t timeout = I2C_TIMEOUT_COUNT;

    while (timeout--)
    {
        uint32_t sr1 = I2C1->SR1;
        i2c_status_t status = I2C_OK;

        /* Success */
        if (sr1 & flag)
        {
            return I2C_OK;
        }

        if (sr1 & I2C_SR1_BERR)
        {
            status = I2C_ERR_BUS;
        }
        else if (sr1 & I2C_SR1_ARLO)
        {
            status = I2C_ERR_BUS;
        }
        else if (sr1 & I2C_SR1_AF)
        {
            status = I2C_ERR_NACK;
        }
        else if (sr1 & I2C_SR1_OVR)
        {
            status = I2C_ERR_BUS;
        }

        if (status != I2C_OK)
        {
            i2c_clear_error_flags(sr1);
            s_state = I2C_STATE_ERROR;
            return status;
        }
    }

    s_state = I2C_STATE_ERROR;
    return I2C_ERR_TIMEOUT;
}

static void i2c_clear_error_flags(uint32_t sr1)
{
    /*
     * Clear only the error flags that were present
     * in the captured SR1 snapshot.
     */
    /* Bus Error */
    if ( sr1 & I2C_SR1_BERR)
    {
        I2C1->SR1 &= ~I2C_SR1_BERR;
    }

    /* Arbitration Lost */
    if (sr1 & I2C_SR1_ARLO)
    {
        I2C1->SR1 &= ~I2C_SR1_ARLO;
    }

    /* Acknowledge Failure */
    if (sr1 & I2C_SR1_AF)
    {
        I2C1->SR1 &= ~I2C_SR1_AF;
    }

    /* Overrun */
    if (sr1 & I2C_SR1_OVR)
    {
        I2C1->SR1 &= ~I2C_SR1_OVR;
    }

    s_state = I2C_STATE_ERROR;
}

static void i2c_reset_bus(void)
{
    /* Generate STOP if the peripheral is still master */
    if (I2C1->SR2 & I2C_SR2_MSL)
    {
        I2C1->CR1 |= I2C_CR1_STOP;
    }

    /*
     * Do not modify the driver state here.
     * The caller decides whether recovery was successful.
     */
}


/*    Nucleo-F446RE
    SYSCLK  = 180 MHz
    AHB     = 180 MHz
    APB1    = 45 MHz
    APB2    = 90 MHz
    I2C1 is on APB1
    GPIOA ON 180MHz
*/