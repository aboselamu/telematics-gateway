# F103 CAN Step 2 — 500 kbit/s Silent Loopback

## Existing verified milestone
Both Blue Pills previously returned SYSCLK 72 MHz and PCLK1 36 MHz by GDB.

## Copy only these files into the repository

- `nodes/stm32f103/inc/can_f103.h` (new)
- `nodes/stm32f103/src/can_f103.c` (new)
- `nodes/stm32f103/src/main.c` (replace previous clock-test-only main; keep git commit)
- `nodes/stm32f103/Makefile` (update)

Do not replace existing `clock_f103.c`, `clock_f103.h`, startup file or linker script.

## Test scope
STM32F103 CAN1: PB8/PB9 AFIO remap, 500 kbit/s, accept-all filter0 FIFO0, standard ID data frame, polling TX and RX, silent internal loopback. No CAN traffic on CANH/CANL in this mode.

Clock: PCLK1=36MHz, BRP=4, BS1=15, BS2=2, SJW=1; CAN_BTR=0xC01E0003.

The wait guards are spin counts, not timed milliseconds. Hardware timing verification and proper timeout supervision belong in later stages.

## Build and flash (one node at a time)

From repository root on Windows CMD:

```
make -C nodes/stm32f103 clean
make -C nodes/stm32f103
make -C nodes/stm32f103 flash
```

Close other OpenOCD sessions before flashing.

Start OpenOCD in one terminal:

```
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg -c "adapter speed 100"
```

Start GDB in another terminal:

```
arm-none-eabi-gdb nodes/stm32f103/build/stm32f103-node.elf
```

Then:

```
target remote localhost:3333
monitor reset halt
continue
```

After a moment press Ctrl+C, then inspect:

```
p g_f103_can_test_done
p g_f103_can_test_pass
p g_f103_can_init_status
p g_f103_can_tx_status
p g_f103_can_rx_status
p/x g_f103_can_btr
p/x g_f103_can_rx_id
p g_f103_can_rx_dlc
p/x g_f103_can_rx_data0
```

Expected values:

```
g_f103_can_test_done   = 1
g_f103_can_test_pass   = 1
init/tx/rx status      = CAN_F103_OK (0)
BTR                    = 0xC01E0003
rx ID                  = 0x123
rx DLC                 = 1
rx data0               = 0x5A
```

Repeat the same binary on the second Blue Pill, connecting the ST-LINK to only the node under test. Verify each result separately.

## Electrical note

The STM32F103 uses PB9 alternate-function **push-pull** into MCP2551 TXD (TTL-level-compatible), unlike the F446's open-drain workaround for its TD521DCAN interface. Silent internal loopback does not verify physical CAN bus wiring, ACK, or transceiver operation. Do not claim multi-node communication yet.
