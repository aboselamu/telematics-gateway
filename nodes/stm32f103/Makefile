# Independent build for STM32F103C8T6 CAN test nodes (two Blue Pills).
# Run from repository root: make -C nodes/stm32f103
# Preserves the root STM32F446 gateway build.

PREFIX      ?= arm-none-eabi-
CC          := $(PREFIX)gcc
OBJCOPY     := $(PREFIX)objcopy
SIZE        := $(PREFIX)size
TARGET      := stm32f103-node
BUILD_DIR   := build

# User's verified on-disk CMSIS header location (F103/Core combined directory)
CMSIS_INC   := third_party/cmsis/Core/Include

ARCH_FLAGS := -mcpu=cortex-m3 -mthumb
CFLAGS     := $(ARCH_FLAGS) -DSTM32F103xB -DHSE_VALUE=8000000U \
              -Iinc -I$(CMSIS_INC) \
              -std=c11 -Wall -Wextra -Werror -g3 -Og \
              -ffreestanding -ffunction-sections -fdata-sections \
              -fno-common
LDFLAGS    := $(ARCH_FLAGS) -nostdlib \
              -Tlinker/stm32f103c8_flash.ld \
              -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--gc-sections \
              -Wl,--print-memory-usage

OBJS := $(BUILD_DIR)/main.o $(BUILD_DIR)/clock_f103.o \
        $(BUILD_DIR)/can_f103.o $(BUILD_DIR)/startup.o

all: $(BUILD_DIR)/$(TARGET).bin size

$(BUILD_DIR)/main.o: src/main.c inc/clock_f103.h inc/can_f103.h
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c src/main.c -o $@

$(BUILD_DIR)/clock_f103.o: src/clock_f103.c inc/clock_f103.h
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c src/clock_f103.c -o $@

$(BUILD_DIR)/can_f103.o: src/can_f103.c inc/can_f103.h
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c src/can_f103.c -o $@

$(BUILD_DIR)/startup.o: startup/startup_stm32f103.s
	mkdir -p $(BUILD_DIR)
	$(CC) $(ARCH_FLAGS) -x assembler-with-cpp -c startup/startup_stm32f103.s -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJS)
	$(CC) $(LDFLAGS) $^ -lgcc -o $@

$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@

size: $(BUILD_DIR)/$(TARGET).elf
	$(SIZE) $<

flash: $(BUILD_DIR)/$(TARGET).elf
	openocd -f interface/stlink.cfg -f target/stm32f1x.cfg -c "adapter speed 100" -c "program $(BUILD_DIR)/$(TARGET).elf verify reset exit"

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean flash size
