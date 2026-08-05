# Telematic Gateway (TG)

> **A production-oriented bare-metal embedded firmware platform built on
> STM32F446RE using CMSIS register-level programming.**

The **Telematic Gateway (TG)** is a long-term embedded systems project
focused on designing reusable, production-quality firmware architectures
rather than isolated peripheral demonstrations.

Instead of simply implementing UART, SPI, I²C or CAN drivers, this
project emphasizes software architecture, modularity, deterministic
state machines, hardware abstraction, and Hardware-in-the-Loop (HIL)
verification.

Every component is developed incrementally and validated on real
hardware before becoming part of the platform.

## Why This Project Exists

Many embedded repositories demonstrate how to make a peripheral work.

This project demonstrates how professional firmware is engineered.

### Focus Areas

-   Reusable drivers
-   Layered architecture
-   Deterministic execution
-   Maintainability
-   Scalability
-   Hardware-in-the-Loop validation

## Design Philosophy

> Correct architecture is more valuable than working code that cannot
> evolve.

### Driver Architecture

``` text
Public API
    │
    ▼
Transaction Managers
    │
    ▼
Primitive Workers
    │
    ▼
STM32 Registers
```

Public APIs validate parameters and dispatch transactions.

Transaction Managers implement protocol sequences.

Primitive Workers perform exactly one hardware operation.

## Engineering Principles

-   Bare-metal CMSIS (No HAL / No LL)
-   Single Responsibility Principle
-   Separation of Policy and Mechanism
-   Deterministic State Machines
-   Centralized Error Handling
-   Reusable Driver Interfaces

## Project Progress

### Phase 0 & 1 --- Foundation ✅

-   Software Architecture
-   Coding Standards
-   Event Queue
-   Ring Buffer

### Phase 2 & 3 --- UART, DMA & Streaming Middleware ✅

-   UART Driver
-   DMA Circular Reception
-   Frame Builder
-   Frame Manager
-   Protocol Parser
-   GPS Decoder
-   End-to-End HIL Validation

### Phase 4 --- I²C Driver ✅

-   Polling Master Driver
-   Read / Write Transactions
-   Repeated START
-   Transaction Managers
-   Primitive Workers
-   RM0390 Event Sequencing
-   HIL Validation

### Planned

-   SPI Driver
-   CAN Driver
-   RTOS Integration
-   Gateway Services
-   Cloud Connectivity

## Hardware Platform

-   STM32F446RE
-   ARM Cortex-M4
-   STM32CubeIDE
-   CMSIS
-   ST-Link V2/V3

## Long-Term Vision

The goal of this repository is to demonstrate how production embedded
firmware is architected from low-level peripheral drivers through
middleware, services, and gateway functionality while validating every
subsystem using real hardware.
