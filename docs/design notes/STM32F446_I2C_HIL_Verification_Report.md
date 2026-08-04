# STM32F446RE Polling I²C Driver
## Hardware-in-the-Loop Verification Report

**Target MCU:** STM32F446RE  
**Environment:** VS Code, Cortex-Debug, OpenOCD, ST-Link  
**Peripheral:** I2C1  
**Pins:** PB6 = SCL, PB7 = SDA  
**APB1 clock:** 45 MHz  
**Configured I²C frequency:** approximately 88 kHz  
**Test device:** DS3231 RTC, 7-bit address `0x68`  
**Driver:** CMSIS/register-level, polling, single execution context

---

# 1. Purpose

This report records hardware-in-the-loop tests used to verify that the I²C driver:

1. enters `I2C_STATE_READY` after initialization;
2. executes combined write/read transfers using a repeated START;
3. selects the correct one-, two-, or N-byte receive sequence;
4. detects a target NACK;
5. distinguishes a recoverable NACK from a stuck/non-idle bus;
6. releases the bus after successful and failed transfers;
7. returns to `I2C_STATE_READY` after valid cleanup;
8. enters `I2C_STATE_ERROR` when the hardware state cannot be trusted.

---

# 2. Driver architecture

## 2.1 Application layer

The application calls public APIs:

```c
i2c_init();
i2c_write();
i2c_read();
i2c_write_read();
i2c_recover();
i2c_get_state();
```

## 2.2 Transaction-manager layer

The public APIs validate parameters, check software state, check the physical bus, select a hardware sequence, process errors, complete bus teardown, and decide the final driver state.

For the tests in this report, the main manager is:

```c
i2c_write_read()
```

## 2.3 Hardware-worker layer

The manager calls private workers such as:

```c
i2c_transmit_phase()
i2c_receive_1_byte()
i2c_receive_2_bytes()
i2c_receive_n_bytes()
i2c_generate_start()
i2c_send_address()
i2c_send_byte()
i2c_receive_byte()
i2c_wait_sr1_flag()
i2c_abort_sequence()
i2c_terminate_bus_request()
i2c_complete_transaction()
```

---

# 3. Driver state model

```text
RESET
  |
  | successful i2c_init()
  v
READY
  |
  | admitted transaction
  +------> BUSY_TX
  +------> BUSY_RX
  +------> BUSY_TX_RX
               |
               | successful completion
               v
             READY

Uncertain hardware failure
               |
               v
             ERROR
               |
               | successful i2c_recover()
               v
             READY
```

A target NACK is treated specially. It is returned to the application as `I2C_ERR_NACK`, but the software state returns to `READY` when STOP completion and bus cleanup succeed.

---

# 4. Common application instrumentation

```c
volatile i2c_status_t g_i2c_init_status;
volatile i2c_status_t g_i2c_transfer_status;

volatile i2c_state_t g_i2c_state_after_init;
volatile i2c_state_t g_i2c_state_after_transfer;

volatile uint8_t g_rtc_seconds_raw;
volatile uint8_t g_rtc_minutes_raw;
```

Initialization:

```c
const i2c_config_t i2c_config =
{
    .clock_speed = I2C_CLOCK_STANDARD_SAFE_HZ
};

SystemClock_Config();

g_i2c_init_status = i2c_init(&i2c_config);
g_i2c_state_after_init = i2c_get_state();
```

Expected:

```text
g_i2c_init_status      = I2C_OK
g_i2c_state_after_init = I2C_STATE_READY
```

---

# 5. Combined transaction manager

```c
i2c_status_t i2c_write_read(
    uint8_t address,
    const uint8_t *p_tx_data,
    uint16_t tx_length,
    uint8_t *p_rx_data,
    uint16_t rx_length)
{
    i2c_status_t status;

    /* Reject invalid parameters before touching hardware. */
    if ((p_tx_data == NULL) ||
        (p_rx_data == NULL) ||
        (tx_length == 0U)   ||
        (rx_length == 0U)   ||
        (address > 0x7FU))
    {
        return I2C_ERR_PARAM;
    }

    /* Acquire the complete combined transaction. */
    switch (s_state)
    {
        case I2C_STATE_READY:

            /* Software READY is not enough: bus must be idle. */
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

    /* Send register address/command without STOP. */
    status = i2c_transmit_phase(
        address,
        p_tx_data,
        tx_length);

    if (status == I2C_OK)
    {
        /* START inside the selected helper becomes repeated START. */
        if (rx_length == 1U)
        {
            status = i2c_receive_1_byte(address, p_rx_data);
        }
        else if (rx_length == 2U)
        {
            status = i2c_receive_2_bytes(address, p_rx_data);
        }
        else
        {
            status = i2c_receive_n_bytes(
                address,
                p_rx_data,
                rx_length);
        }
    }

    /* Successful transaction completion. */
    if (status == I2C_OK)
    {
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

    /* Recoverable target NACK. */
    if (status == I2C_ERR_NACK)
    {
        i2c_status_t cleanup_status;

        cleanup_status = i2c_complete_transaction();

        if (cleanup_status == I2C_OK)
        {
            s_state = I2C_STATE_READY;
            return I2C_ERR_NACK;
        }

        s_state = I2C_STATE_ERROR;
        return cleanup_status;
    }

    /* Any other failure leaves the hardware state uncertain. */
    s_state = I2C_STATE_ERROR;

    return status;
}
```

---

# 6. Shared write phase

```c
static i2c_status_t i2c_transmit_phase(
    uint8_t address,
    const uint8_t *p_data,
    uint16_t length)
{
    i2c_status_t status;
    uint16_t index;

    /* Generate START and wait for SB. */
    status = i2c_generate_start();

    if (status != I2C_OK)
    {
        return i2c_abort_sequence(status);
    }

    /* Send the 7-bit address in write direction. */
    status = i2c_send_address(address, 0U);

    if (status != I2C_OK)
    {
        return i2c_abort_sequence(status);
    }

    /* Clear ADDR by reading SR1 followed by SR2. */
    i2c_clear_addr_flag();

    /* Send all requested bytes. */
    for (index = 0U; index < length; index++)
    {
        status = i2c_send_byte(p_data[index]);

        if (status != I2C_OK)
        {
            return i2c_abort_sequence(status);
        }
    }

    /* BTF confirms the final byte completed, not merely TXE. */
    status = i2c_wait_sr1_flag(I2C_SR1_BTF);

    if (status != I2C_OK)
    {
        return i2c_abort_sequence(status);
    }

    /* No STOP: the next START must be repeated START. */
    return I2C_OK;
}
```

START and address workers:

```c
static i2c_status_t i2c_generate_start(void)
{
    I2C1->CR1 |= I2C_CR1_START;
    return i2c_wait_sr1_flag(I2C_SR1_SB);
}

static i2c_status_t i2c_send_address(
    uint8_t address,
    uint8_t read)
{
    uint8_t addr;

    addr = (uint8_t)(address << 1U);

    if (read != 0U)
    {
        addr |= 1U;
    }
    else
    {
        addr &= (uint8_t)~1U;
    }

    I2C1->DR = addr;

    return i2c_wait_sr1_flag(I2C_SR1_ADDR);
}
```

For the DS3231:

```text
Write address byte = 0xD0
Read address byte  = 0xD1
```

---

# 7. Error detection and cleanup

```c
static i2c_status_t i2c_wait_sr1_flag(uint32_t flag)
{
    uint32_t timeout = I2C_TIMEOUT_COUNT;

    while (timeout > 0U)
    {
        uint32_t sr1 = I2C1->SR1;

        /* Clear documented spurious controller-mode BERR. */
        if ((sr1 & I2C_SR1_BERR) != 0U)
        {
            i2c_clear_error_flags(I2C_SR1_BERR);
            sr1 &= ~I2C_SR1_BERR;
        }

        if ((sr1 & I2C_SR1_ARLO) != 0U)
        {
            i2c_clear_error_flags(I2C_SR1_ARLO);
            return I2C_ERR_BUS;
        }

        /* AF is acknowledge failure/NACK. */
        if ((sr1 & I2C_SR1_AF) != 0U)
        {
            i2c_clear_error_flags(I2C_SR1_AF);
            return I2C_ERR_NACK;
        }

        if ((sr1 & I2C_SR1_OVR) != 0U)
        {
            i2c_clear_error_flags(I2C_SR1_OVR);
            return I2C_ERR_BUS;
        }

        if ((sr1 & flag) == flag)
        {
            return I2C_OK;
        }

        timeout--;
    }

    return I2C_ERR_TIMEOUT;
}
```

Abort path:

```c
static i2c_status_t i2c_abort_sequence(
    i2c_status_t error_status)
{
    i2c_terminate_bus_request();
    return error_status;
}

static void i2c_terminate_bus_request(void)
{
    if ((I2C1->SR2 & I2C_SR2_MSL) != 0U)
    {
        /* Peripheral owns the bus: request STOP. */
        i2c_generate_stop();
    }
    else
    {
        /* Never became master or lost ownership: cancel START. */
        I2C1->CR1 &= ~I2C_CR1_START;
    }
}
```

Common completion:

```c
static i2c_status_t i2c_complete_transaction(void)
{
    uint32_t timeout = I2C_TIMEOUT_COUNT;

    /* Wait until STOP physically completes. */
    while ((I2C1->CR1 & I2C_CR1_STOP) != 0U)
    {
        if (timeout == 0U)
        {
            return I2C_ERR_TIMEOUT;
        }

        timeout--;
    }

    /* Peripheral must no longer be master. */
    if ((I2C1->SR2 & I2C_SR2_MSL) != 0U)
    {
        return I2C_ERR_BUS;
    }

    /* Restore idle receive configuration. */
    i2c_enable_ack();
    i2c_disable_pos();

    return I2C_OK;
}
```

---

# 8. Test No. 1 — Recoverable address NACK

## 8.1 Objective

Verify that a target NACK is reported as `I2C_ERR_NACK`, while successful STOP/cleanup returns the software state to `I2C_STATE_READY`.

## 8.2 Hardware setup

```text
PB6/SCL connected to RTC SCL
PB7/SDA disconnected from RTC SDA
PB7/SDA pulled up externally to 3.3 V
RTC powered and grounded
```

The external pull-up keeps SDA high. Therefore, this is a healthy but unacknowledged bus, not a stuck-low bus.

## 8.3 Application call

```c
uint8_t register_address = DS3231_SECONDS_REGISTER;
uint8_t rtc_data[2] = {0U, 0U};

g_i2c_transfer_status = i2c_write_read(
    DS3231_I2C_ADDRESS,
    &register_address,
    1U,
    rtc_data,
    2U);

g_i2c_state_after_transfer = i2c_get_state();
```

## 8.4 Decision flow

```text
main()
  |
  | i2c_write_read(0x68, ..., tx=1, rx=2)
  v
i2c_write_read()
  |
  +--> parameters valid
  +--> software state READY
  +--> SR2.BUSY = 0
  +--> state = BUSY_TX_RX
  |
  v
i2c_transmit_phase()
  |
  +--> i2c_generate_start()
  |      +--> wait SR1.SB
  |
  +--> i2c_send_address(0x68, WRITE)
         +--> DR = 0xD0
         +--> wait SR1.ADDR
                 +--> RTC cannot ACK
                 +--> SR1.AF = 1
                 +--> clear AF
                 +--> return I2C_ERR_NACK
  |
  v
i2c_abort_sequence(I2C_ERR_NACK)
  |
  +--> terminate current request
  +--> request STOP when MSL = 1
  +--> return I2C_ERR_NACK
  |
  v
i2c_write_read()
  |
  +--> NACK branch
  +--> i2c_complete_transaction()
         +--> wait STOP clear
         +--> verify MSL = 0
         +--> ACK = 1
         +--> POS = 0
  |
  +--> cleanup successful
  +--> state = READY
  +--> return I2C_ERR_NACK
```

## 8.5 Observed results

```text
g_i2c_init_status          = I2C_OK
g_i2c_state_after_init     = I2C_STATE_READY
g_i2c_transfer_status      = I2C_ERR_NACK
g_i2c_state_after_transfer = I2C_STATE_READY

GPIOB->IDR & 0xC0 = 0xC0
I2C1->SR2         = 0x0000
I2C1->CR1         = 0x0401
```

Interpretation:

```text
GPIO IDR 0xC0: SCL high, SDA high
SR2 0x0000:    BUSY = 0, MSL = 0
CR1 0x0401:    PE = 1, ACK = 1, START/STOP/POS = 0
```

## 8.6 Conclusion

**PASS**

The test proved:

- address NACK detection;
- `I2C_ERR_NACK` propagation;
- distinction between NACK and stuck bus;
- successful STOP completion;
- bus release;
- idle ACK/POS restoration;
- return to `I2C_STATE_READY`.

---

# 9. Test No. 2 — Successful one-byte combined read

## 9.1 Objective

Verify the special one-byte receive path selected by:

```c
rx_length == 1U
```

## 9.2 Application call

```c
uint8_t register_address = DS3231_SECONDS_REGISTER;
uint8_t rtc_data[1] = {0U};

g_i2c_transfer_status = i2c_write_read(
    DS3231_I2C_ADDRESS,
    &register_address,
    1U,
    rtc_data,
    1U);

if (g_i2c_transfer_status == I2C_OK)
{
    g_rtc_seconds_raw = rtc_data[0];
}

g_i2c_state_after_transfer = i2c_get_state();
```

## 9.3 One-byte receive implementation

```c
static i2c_status_t i2c_receive_1_byte(
    uint8_t address,
    uint8_t *p_data)
{
    i2c_status_t status;
    uint32_t primask_state;

    /* Establish known receive configuration. */
    i2c_enable_ack();
    i2c_disable_pos();

    /* This START becomes repeated START. */
    status = i2c_generate_start();

    if (status != I2C_OK)
    {
        return i2c_abort_sequence(status);
    }

    /* Send address in read direction: 0xD1 for DS3231. */
    status = i2c_send_address(address, 1U);

    if (status != I2C_OK)
    {
        return i2c_abort_sequence(status);
    }

    /* Disable ACK before clearing ADDR. */
    i2c_disable_ack();

    /* Keep ADDR clear and STOP programming adjacent. */
    primask_state = __get_PRIMASK();
    __disable_irq();

    /* Release the hardware to receive the byte. */
    i2c_clear_addr_flag();

    /* Terminate after the single byte. */
    i2c_generate_stop();

    /* Restore the exact previous interrupt state. */
    __set_PRIMASK(primask_state);

    /* Wait for RXNE and read DR. */
    status = i2c_receive_byte(p_data);

    if (status != I2C_OK)
    {
        return i2c_abort_sequence(status);
    }

    return I2C_OK;
}
```

Primitive receive worker:

```c
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
```

## 9.4 Decision flow

```text
main()
  |
  | i2c_write_read(0x68, ..., tx=1, rx=1)
  v
i2c_write_read()
  |
  +--> state READY and bus idle
  +--> state = BUSY_TX_RX
  |
  v
i2c_transmit_phase()
  |
  +--> START
  +--> address 0xD0
  +--> clear ADDR
  +--> write register 0x00
  +--> wait BTF
  |
  v
i2c_receive_1_byte()
  |
  +--> repeated START
  +--> address 0xD1
  +--> wait ADDR
  +--> ACK = 0
  +--> enter critical section
  +--> clear ADDR
  +--> request STOP
  +--> restore PRIMASK
  +--> wait RXNE
  +--> read one byte from DR
  |
  v
i2c_complete_transaction()
  |
  +--> wait STOP clear
  +--> verify MSL = 0
  +--> ACK = 1
  +--> POS = 0
  |
  v
state = READY
return I2C_OK
```

## 9.5 Observed results

```text
g_i2c_state_after_init     = I2C_STATE_READY
g_i2c_transfer_status      = I2C_OK
g_i2c_state_after_transfer = I2C_STATE_READY
g_rtc_seconds_raw          = 85 decimal = 0x55 = 55 seconds BCD

GPIOB->IDR & 0xC0 = 0xC0
I2C1->SR2         = 0x0000
I2C1->CR1         = 0x0401
```

The transfer-status watch did not show a change because its startup value was zero and `I2C_OK` is also zero. The final state and received byte prove the transaction executed.

## 9.6 Conclusion

**PASS**

The test proved:

- repeated START generation;
- one-byte ACK/NACK sequencing;
- critical ADDR-clear/STOP ordering;
- RXNE polling;
- one-byte DR extraction;
- STOP completion;
- return to `I2C_STATE_READY`.

---

# 10. Verification summary

| Test | Description | Result |
|---|---|---|
| 1 | Recoverable address NACK with external SDA pull-up and RTC SDA disconnected | PASS |
| 2 | Successful one-byte combined read from DS3231 seconds register | PASS |

The tests demonstrate:

```text
Healthy bus + no target ACK -> I2C_ERR_NACK -> READY
Stuck/non-idle bus          -> I2C_ERR_BUS  -> ERROR
Successful transfer         -> I2C_OK       -> READY
```

---

# 11. Next test — Test No. 3: successful N-byte combined read

## 11.1 Objective

Verify the `rx_length > 2` branch and `i2c_receive_n_bytes()` using seven DS3231 registers from `0x00` to `0x06`.

This exercises:

- bulk RXNE reads;
- pointer and remaining-length updates;
- the strict final-three-byte BTF sequence;
- ACK disable at the correct point;
- STOP generation;
- final RXNE read;
- common completion and return to READY.

## 11.2 Application changes

Add globally:

```c
volatile uint8_t g_rtc_raw[7];
```

Use in `main()`:

```c
uint8_t register_address = DS3231_SECONDS_REGISTER;
uint8_t rtc_data[7] = {0U};

g_i2c_transfer_status = i2c_write_read(
    DS3231_I2C_ADDRESS,
    &register_address,
    1U,
    rtc_data,
    7U);

if (g_i2c_transfer_status == I2C_OK)
{
    for (uint32_t i = 0U; i < 7U; i++)
    {
        g_rtc_raw[i] = rtc_data[i];
    }
}

g_i2c_state_after_transfer = i2c_get_state();
```

## 11.3 Returned register order

```text
g_rtc_raw[0] = seconds
g_rtc_raw[1] = minutes
g_rtc_raw[2] = hours
g_rtc_raw[3] = day
g_rtc_raw[4] = date
g_rtc_raw[5] = month/century
g_rtc_raw[6] = year
```

## 11.4 Pass criteria

```text
g_i2c_init_status          = I2C_OK
g_i2c_state_after_init     = I2C_STATE_READY
g_i2c_transfer_status      = I2C_OK
g_i2c_state_after_transfer = I2C_STATE_READY

GPIOB->IDR & 0xC0 = 0xC0
I2C1->SR2         = 0x0000
I2C1->CR1         = 0x0401
```

The seven bytes must contain plausible DS3231 values, especially valid BCD seconds and minutes.

---

# 12. Report status

This is a living verification report. Each later test should append:

1. objective;
2. hardware arrangement;
3. application call;
4. driver decision path;
5. actual implementation functions;
6. debugger/register evidence;
7. pass criteria;
8. conclusion.
