# Telematics Gateway

**A bare-metal, event-driven embedded gateway engineered for high-throughput telematics data routing.** Built on the STM32F4 architecture, this project focuses on non-blocking peripheral drivers, lock-free middleware, and strict Hardware-in-the-Loop (HIL) verification to ensure enterprise-grade stability in automotive and industrial environments.

---

## 🏗️ System Architecture & Project Status

### Phase 1: Event Queue Framework
**Status:** Completed & Verified

* **Architecture:** Engineered a robust, lock-free middleware layer to decouple hardware interrupts (ISRs) from the main application loop. Implemented custom zero-copy event envelopes to pass state data without memory fragmentation.
* **Verification:** Fully unit-tested the circular buffer wrap-around math and queue saturation logic using a desktop-based Python testing environment.

### Phase 2: Production-Grade UART Driver
**Status:** Completed & Verified

* **Architecture:** Engineered a non-blocking, interrupt-driven USART2 module. Implemented a dual circular ring buffer (256-byte TX/RX) utilizing a **decoupled cursor reading strategy** to eliminate ISR race conditions. Application layer extraction is managed via a lock-free claim-ticket notification system, completely isolating the main loop from hardware timing constraints.
* **Verification:** Passed a 500-cycle Hardware-in-the-Loop (HIL) rapid-fire stress test using a custom Python PySerial desktop script. 
* **Result:** Zero dropped frames, zero buffer overruns, and successful atomic state synchronization maintained under heavy, continuous physical load at 115200 baud.

---

## 📂 Project Structure 

```text
telematics-gateway/
├── app/
│   └── main.c                 # Application entry point and event dispatcher
├── middleware/
│   ├── event_queue.h          # Lock-free event queue public APIs
│   └── event_queue.c          # Event queue implementation
├── drivers/
│   ├── uart_driver.h          # Hardware-abstracted UART APIs
│   └── uart_driver.c          # Interrupt-driven STM32F4 USART2 implementation
└── tests/
    ├── unit/                  # Desktop-based C unit tests
    └── hil/                   
        └── test_uart_hil.py   # Python PySerial automated hardware stress tests

```
---
## 🚀  Development Roadmap
Future phases are actively managed via Jira sprints, progressing toward a fully integrated cloud-connected gateway:

[x] Phase 0 & 1: Project Architecture & Event Queue Framework

[x] Phase 2: Production-Grade UART Drivers

[ ] Phase 3: Direct Memory Access (DMA) Integration

[ ] Phases 4 - 6: SPI Architecture & CAN Bus Communication

[ ] Phases 7 - 11: Services Layer, RTOS Migration, & Cloud Gateway Integration

Developed as a demonstration of robust embedded C architecture, defensive programming, and automated hardware testing methodologies.

---

## This project is managed using Jira via the official GitHub-Jira integration. Commits, feature branches, and pull requests are linked to structured Jira tickets to maintain a strict, agile software development lifecycle.