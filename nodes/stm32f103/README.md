# STM32F103 Blue Pill — Clock Bring-up (Step 1)

This directory is a **separate firmware target** in the Telematics Gateway repository.
It does not alter the verified F446 gateway firmware.

## Prerequisite: STM32F1 CMSIS device headers

The repository already contains common CMSIS Core headers at
`third_party/cmsis/Core/Include`. The STM32F103 requires **STM32F1** device
headers; F446 headers cannot be reused.

In the *repository root* run:

```bash
git submodule add https://github.com/STMicroelectronics/cmsis-device-f1.git third_party/cmsis/Device/STSTM32F1xx
```

This provides `stm32f1xx.h`, `stm32f103xb.h`, and `system_stm32f1xx.h`
in `third_party/cmsis/Device/STSTM32F1xx/Include`.
Commit the generated `.gitmodules` and the submodule pointer with your project.
Other clones can restore it using `git submodule update --init --recursive`.

## Build

```bash
make -C nodes/stm32f103
```

Expected output: `nodes/stm32f103/build/stm32f103-node.elf` (and `.bin`).

## Flash using the external ST-LINK V2

Disconnect the GDB session and close the current OpenOCD process before running:

```bash
make -C nodes/stm32f103 flash
```

If you prefer GDB `load`, start OpenOCD separately instead of running `flash`.
Do not try to run a second OpenOCD process at the same time.

## Debug (OpenOCD in one terminal)

```bash
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg -c "adapter speed 100"
```

In a **second** terminal, from the repository root:

```bash
arm-none-eabi-gdb nodes/stm32f103/build/stm32f103-node.elf
```

Then:

```gdb
target remote localhost:3333
monitor reset halt
continue
# After the firmware runs, interrupt with Ctrl+C
p g_f103_clock_done
p g_f103_clock_status
p SystemCoreClock
p g_f103_can_pclk1_hz
p/x g_f103_rcc_cr
p/x g_f103_rcc_cfgr
p/x g_f103_flash_acr
```

Expected:
- `g_f103_clock_done = 1`
- `g_f103_clock_status = 0` (success)
- `SystemCoreClock = 72000000`
- `g_f103_can_pclk1_hz = 36000000`
- `RCC_CFGR.SWS = PLL`, APB1 prescaler = /2
- `FLASH_ACR.LATENCY = 2` (two wait states; field value `0x2`)

If HSE startup fails, status will be nonzero. This project assumes the Blue
Pill has a working **8 MHz external HSE crystal**. The HSE/PLL waits have
finite timeouts so the firmware can report failure rather than hang forever.

## Hardware note

This step does **not** initialize bxCAN, transmit anything or require
physical CAN wiring changes. Keep the F446 firmware unchanged.

After confirming the clock registers in GDB, the next step is to initialize
F103 bxCAN in internal loopback at 500 kbit/s, including the F103-specific
AFIO PB8/PB9 remap. The STM32F103 CAN TX/RX IRQ names differ from the F446;
this startup vector table uses the F103-specific names.
