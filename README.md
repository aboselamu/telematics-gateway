# Telematics Gateway

> **A production-oriented, bare-metal embedded gateway built on the
> STM32F4 platform, demonstrating scalable firmware architecture,
> hardware abstraction, defensive driver design, and event-driven
> communication for automotive and industrial telematics systems.**

The project is developed incrementally using a structured
architecture-first methodology. Every subsystem progresses through
requirements, architecture, implementation, verification, and
Hardware-in-the-Loop (HIL) validation before integration.

------------------------------------------------------------------------

# Project Objectives

This repository is not intended to demonstrate isolated peripheral
examples.

Instead, it focuses on designing a reusable firmware architecture
capable of scaling from a single UART interface into a complete
multi-protocol telematics gateway supporting:

-   UART
-   DMA
-   I²C
-   SPI
-   CAN Bus
-   External Sensors
-   LTE Modem
-   GPS Receiver
-   Cloud Connectivity

while maintaining deterministic execution, predictable memory usage, and
complete separation between hardware and application logic.

------------------------------------------------------------------------

# Firmware Design Principles

## Event-Driven Architecture

Hardware peripherals never execute application logic directly.

``` text
Peripherals
      │
      ▼
Interrupt Service Routines
      │
      ▼
Event Queue
      │
      ▼
Dispatcher
      │
      ▼
Application Services
```

## Memory Ownership

DMA buffers belong exclusively to the transport layer.

Application code never parses volatile DMA memory.

``` text
DMA Circular Buffer
        │
        ▼
Frame Builder
        │
        ▼
Static Frame Buffer
        │
        ▼
Event Queue
        │
        ▼
Protocol Parser
```

## Hardware Abstraction

Application code communicates only through driver interfaces.

``` text
Application
    │
    ▼
Driver API
    │
    ▼
Driver Services
    │
    ▼
Hardware Primitives
    │
    ▼
STM32 Registers
```

## Defensive Programming

-   Parameter validation
-   Driver state management
-   Software timeout protection
-   Centralized error detection
-   Bus recovery mechanisms
-   Static memory allocation
-   No dynamic memory allocation

------------------------------------------------------------------------

# Development Progress

## Phase 0 --- Project Foundation

**Status:** ✅ Completed

Repository structure, architecture, Git workflow, Jira integration and
coding conventions established.

## Phase 1 --- Event Queue Framework

**Status:** ✅ Completed & Verified

-   Lock-free circular queue
-   Static memory allocation
-   Zero-copy event envelopes
-   ISR-safe producer model

Verified through desktop testing of queue wrap-around, saturation and
boundary conditions.

## Phase 2 --- Production UART Driver

**Status:** ✅ Completed & Hardware Verified

-   Interrupt-driven UART
-   Dual circular ring buffers
-   Non-blocking API
-   Event-driven notifications

Verified with Hardware-in-the-Loop testing:

-   STM32F446RE
-   Python + PySerial
-   500 stress cycles
-   Zero dropped frames
-   Zero buffer overruns

## Phase 3 --- DMA Integration & Data Pipeline Architecture

**Status:** 🚧 In Progress

Current architectural milestones:

-   DMA-driven UART reception
-   Memory Ownership architecture
-   Frame Builder abstraction
-   Static frame buffers
-   Zero-copy event notifications
-   Production-grade I²C driver foundation

------------------------------------------------------------------------

# Project Structure

``` text
telematics-gateway/
├── app/
├── middleware/
│   ├── event_queue.h
│   └── event_queue.c
├── drivers/
│   ├── uart_driver.*
│   ├── dma_driver.*
│   └── i2c_driver.*
├── tests/
│   ├── unit/
│   └── hil/
└── docs/
```

------------------------------------------------------------------------

# Engineering Workflow

``` text
Requirements
      │
      ▼
Architecture
      │
      ▼
Public API
      │
      ▼
Driver Context
      │
      ▼
Implementation
      │
      ▼
Unit Verification
      │
      ▼
Hardware-in-the-Loop Testing
      │
      ▼
Production Integration
```

------------------------------------------------------------------------

# Development Roadmap

  Phase                                   Status
  --------------------------------------- --------
  Phase 0 --- Project Foundation          ✅
  Phase 1 --- Event Queue Framework       ✅
  Phase 2 --- UART Driver                 ✅
  Phase 3 --- DMA & Driver Architecture   🚧
  Phase 4 --- SPI Driver                  ⏳
  Phase 5 --- CAN Driver                  ⏳
  Phase 6 --- I²C Integration             ⏳
  Phase 7 --- Sensor Services             ⏳
  Phase 8 --- Telematics Services         ⏳
  Phase 9 --- RTOS Migration              ⏳
  Phase 10 --- Cloud Gateway              ⏳
  Phase 11 --- Production Optimization    ⏳

------------------------------------------------------------------------

# Project Management

Development is managed using **Jira** integrated with **GitHub**.

Every feature follows:

-   Requirements
-   Design
-   Implementation
-   Code Review
-   Hardware Validation
-   Completion

Branches, commits and pull requests are linked to Jira issues to provide
end-to-end traceability throughout the firmware development lifecycle.
