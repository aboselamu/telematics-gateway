# Telematic Gateway (TG)

> **A bare-metal embedded firmware platform for STM32F446RE, developed with CMSIS register-level programming and verified incrementally on real hardware.**

The **Telematic Gateway (TG)** is a long-term embedded systems project focused on building a reusable firmware architecture rather than a collection of isolated peripheral demonstrations.

The project is developed incrementally from low-level peripheral drivers through device drivers, middleware, services, and eventually gateway functionality. Each subsystem is given clear ownership boundaries, explicit state and error semantics, and Hardware-in-the-Loop (HIL) verification before the next layer is added.

The current platform is intentionally **bare-metal**: no STM32 HAL and no LL drivers.

---

## Why This Project Exists

Many embedded examples stop when a peripheral can transmit or receive data.

TG focuses on what comes after bring-up:

- Who owns an asynchronous transaction after an API returns?
- Where does state live while hardware continues running?
- How should interrupt completion cross software layers?
- Which failures belong to the transport, device, middleware, or application?
- How can a subsystem be verified on real hardware before the architecture grows?

The objective is to make those decisions explicit and repeatable.

---

## Architecture

```text
Application / Gateway Services
            │
            ▼
      Device / Sensor Services
            │
            ▼
          Middleware
  ┌─────────┼──────────────┐
  │         │              │
Frame    Protocol       Event Queue
Manager  Parsers
            │
            ▼
        Device Drivers
      ┌─────┴─────┐
      │           │
   DS3231      Future Devices
      │
      ▼
     Peripheral Drivers
  ┌────┬────┬────┬────┐
 UART DMA   I²C  CAN
  └────┴────┴────┴────┘
            │
            ▼
   CMSIS Register Layer
            │
            ▼
      STM32F446RE
```

### Peripheral Driver Pattern

```text
Public API
    │
    ▼
Transaction / State Management
    │
    ▼
Interrupt or Polling Progression
    │
    ▼
Primitive Register Operations
    │
    ▼
STM32 Registers
```

Public APIs validate requests and establish ownership.

Transaction/state logic preserves context across asynchronous execution.

Primitive workers perform narrowly scoped hardware operations.

---

## Engineering Principles

- Bare-metal CMSIS register-level development
- No HAL / No LL
- Clear ownership of asynchronous operations
- Separation of policy and mechanism
- Explicit state machines
- Small ISR responsibilities
- Layer-independent peripheral drivers
- Device-specific logic outside transport drivers
- Centralized status and error handling
- Hardware-in-the-Loop verification
- Deliberate v1 scope and documented hardening backlog

---

## Asynchronous Completion Model

The I²C work established the completion pattern used by the platform:

```text
Peripheral Hardware
        │
        ▼
       ISR
        │
        ▼
Transaction Completion
        │
        ▼
     Callback
        │
        ▼
    Event Queue
        │
        ▼
Device / Application Layer
```

The peripheral driver does **not** depend directly on middleware. Completion crosses the layer boundary through a small callback contract, while deferred processing happens outside interrupt context.

A useful rule from the I²C implementation is:

> **The peripheral driver understands the bus.  
> The device driver understands the device.  
> The application should understand the information it needs.**

---

## Project Progress

### Phase 0 & 1 — Foundation ✅

- Software architecture
- Coding conventions
- Event queue
- Ring buffer
- Repository/build structure

### Phase 2 & 3 — UART, DMA & Streaming Middleware ✅

- UART driver
- DMA circular reception
- UART refactor for DMA integration
- Frame Builder
- Frame Manager
- Protocol Parser
- GPS Decoder
- End-to-End streaming HIL validation

### Phase 4 — Sensor Middleware / I²C ✅

#### I²C transport

- Polling master driver
- Interrupt-driven non-blocking master driver
- Standalone write and read
- Combined write-read
- Repeated START
- 1-byte, 2-byte and multi-byte receive paths
- Transaction state separated from protocol phase
- NACK handling
- BUSY-path handling
- Callback-based completion boundary
- Event queue integration

#### DS3231 device layer

- Asynchronous time read
- Asynchronous time write
- BCD encode/decode
- 12/24-hour decode handling
- Date/time validation
- Device-level state machine
- Write/read-back verification on real hardware

#### HIL results

Interrupt-driven I²C verification:

```text
HIL tests passed:             9
HIL tests failed:             0
Stress transactions:    10,000 / 10,000
Event-post failures:          0
Final transport result:  I2C_OK
```

DS3231 verification:

```text
Async write:            PASS
Async read:             PASS
Write/read-back:        PASS
BCD encode/decode:      PASS
Device state flow:      PASS
Callback/event path:    PASS
```

These results describe the completed HIL campaign and its defined test scope; they are not a blanket reliability claim.

### Phase 5 — CAN Communication Subsystem 🚧

Current next milestone:

- CAN controller bring-up and bit timing
- Multi-node CAN TX/RX baseline
- Interrupt-driven CAN and event integration
- Acceptance filtering
- Error-state handling
- Error-passive / bus-off detection
- Bus-off recovery
- Multi-node CAN HIL verification
- CAN v1 freeze after defined verification scope passes

The initial CAN scope is intentionally limited to **Classic CAN, 11-bit identifiers, data frames, and a small multi-node bench** before higher-level protocols are introduced.

### Later Phases

- SPI integration
- Gateway services
- RTOS integration
- Higher-level CAN protocols as required
- Diagnostics
- Cloud / external gateway connectivity

---

## Current CAN Test Direction

The next hardware stage is a real multi-node CAN bench rather than loopback-only testing.

Planned topology:

```text
STM32F446RE
    │
CAN Controller
    │
CAN Transceiver
    │
    ├──────── CANH ────────────────────────┐
    └──────── CANL ────────────────────────┤
                                           │
Additional MCU Node                        │
    │                                      │
CAN Transceiver                            │
    │                                      │
    ├──────── CANH ────────────────────────┤
    └──────── CANL ────────────────────────┤
                                           │
Independent CAN Observer / Third Node ─────┘
```

The HIL campaign will grow progressively from basic frame exchange into filtering, arbitration, induced errors, bus-off/recovery, and stress traffic.

---

## Verification Strategy

Verification is split according to what is being tested:

```text
Pure computation
      │
      ▼
 Unit Tests

Peripheral / Device Interaction
      │
      ▼
     HIL

Complete asynchronous subsystem
      │
      ▼
Integration HIL
```

Examples of pure computation include BCD conversion, scaling, CRC, packet decoding, and range validation.

Hardware-dependent behaviour such as interrupt sequencing, bus timing, NACK handling, peripheral status flags, and physical-device interaction belongs in HIL.

---

## Hardware Platform

Primary platform:

- STM32F446RE
- ARM Cortex-M4
- STM32 Nucleo development board
- CMSIS
- ST-Link
- GCC / `arm-none-eabi-gcc`

Current verified peripheral hardware includes:

- DS3231 RTC
- I²C bench wiring and HIL setup

CAN hardware is being expanded into a multi-node bench using external CAN transceivers.

---

## Repository Direction

The repository is intended to evolve from low-level hardware access toward a complete gateway architecture while keeping subsystem boundaries visible.

The guiding progression is:

```text
Make the hardware work
        ↓
Define ownership
        ↓
Make execution asynchronous
        ↓
Make failure semantics explicit
        ↓
Integrate through clean boundaries
        ↓
Verify on real hardware
        ↓
Freeze the verified scope
        ↓
Move upward in the architecture
```

The goal is not to maximize the number of supported peripherals.

The goal is to build a firmware platform in which each new subsystem is easier to reason about because the boundaries established by the previous one remain clear.

---

## Status

**Current completed milestone:** Interrupt-driven I²C + DS3231 device layer with HIL verification.

**Current development milestone:** Multi-node CAN communication subsystem.

