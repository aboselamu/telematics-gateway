# =============================================================================
# KOAS-V1.0 — Kitchen Order Alert System
# Target: STM32F103C8T6 (Blue Pill), Cortex-M3, 8MHz HSI
# =============================================================================

TARGET    = KOAS_V1
BUILD_DIR = build

# --- Toolchain ---
PREFIX  = arm-none-eabi-
CC      = $(PREFIX)gcc
AS      = $(PREFIX)gcc
OBJCOPY = $(PREFIX)objcopy
SIZE    = $(PREFIX)size

# --- MCU — Cortex-M3, NO FPU (STM32F103 has none) ---
CPU = -mcpu=cortex-m3
MCU = $(CPU) -mthumb

# --- Include paths ---
INCLUDES = \
    -ICore/Inc \
    -IApp/Inc \
    -IDrivers/BSP/Inc \
    -IDrivers/MCU_Drivers/Inc \
    -IDrivers/CMSIS/Device/STM32F1xx/Include \
    -IDrivers/CMSIS/Include

# --- Defines ---
DEFS = -DSTM32F103xB

# --- Compiler flags ---
CFLAGS = $(MCU) $(DEFS) $(INCLUDES) \
    -Wall \
    -Wextra \
    -O0 \
    -g3 \
    -ffunction-sections \
    -fdata-sections

# --- Linker flags ---
LDSCRIPT = startup/stm32f103c8tx_flash.ld

LDFLAGS = \
    $(MCU) \
    -T$(LDSCRIPT) \
    -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--gc-sections \
    --specs=nosys.specs \
    -lc -lm

# --- C source files ---
C_SRCS = \
    Core/Src/main.c \
    Core/Src/stm32_it.c \
    App/Src/order_alert_fsm.c \
    Drivers/MCU_Drivers/Src/exti_driver.c \
    Drivers/MCU_Drivers/Src/pwm_driver.c \
    Drivers/MCU_Drivers/Src/buzzer_driver.c \
    Drivers/MCU_Drivers/Src/led_driver.c \
    Drivers/MCU_Drivers/Src/ir_sensor_driver.c \
    Drivers/BSP/Src/board.c

# --- Assembly startup ---
ASM_SRC = startup/startup_stm32f103xb.s

# --- Object files (all flat in build/ — avoids Windows mkdir -p issues) ---
C_OBJS = \
    $(BUILD_DIR)/main.o \
    $(BUILD_DIR)/stm32_it.o \
    $(BUILD_DIR)/order_alert_fsm.o \
    $(BUILD_DIR)/exti_driver.o \
    $(BUILD_DIR)/pwm_driver.o \
    $(BUILD_DIR)/buzzer_driver.o \
    $(BUILD_DIR)/led_driver.o \
    $(BUILD_DIR)/ir_sensor_driver.o \
    $(BUILD_DIR)/board.o

ASM_OBJ = $(BUILD_DIR)/startup.o

ALL_OBJS = $(C_OBJS) $(ASM_OBJ)

# =============================================================================
# Targets
# =============================================================================

all: $(BUILD_DIR) $(BUILD_DIR)/$(TARGET).bin
	@echo ""
	@echo "  Build complete: $(BUILD_DIR)/$(TARGET).bin"
	@echo ""

# Create build directory (simple mkdir, no -p needed since it is flat)
$(BUILD_DIR):
	mkdir $(BUILD_DIR)

# Compile C files — explicit rules, no pattern subdirectory needed
$(BUILD_DIR)/main.o: Core/Src/main.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/stm32_it.o: Core/Src/stm32_it.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/order_alert_fsm.o: App/Src/order_alert_fsm.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/exti_driver.o: Drivers/MCU_Drivers/Src/exti_driver.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/pwm_driver.o: Drivers/MCU_Drivers/Src/pwm_driver.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/buzzer_driver.o: Drivers/MCU_Drivers/Src/buzzer_driver.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/led_driver.o: Drivers/MCU_Drivers/Src/led_driver.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/ir_sensor_driver.o: Drivers/MCU_Drivers/Src/ir_sensor_driver.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/board.o: Drivers/BSP/Src/board.c
	$(CC) $(CFLAGS) -c $< -o $@

# Compile startup assembly
$(BUILD_DIR)/startup.o: startup/startup_stm32f103xb.s
	$(AS) -x assembler-with-cpp $(MCU) -c $< -o $@

# Link
$(BUILD_DIR)/$(TARGET).elf: $(ALL_OBJS)
	$(CC) $(ALL_OBJS) $(LDFLAGS) -o $@
	$(SIZE) $@

# Binary
$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@

# =============================================================================
# Flash via OpenOCD + ST-Link
# =============================================================================
flash: $(BUILD_DIR)/$(TARGET).elf
	openocd -f interface/stlink.cfg \
	        -f target/stm32f1x.cfg  \
	        -c "program $(BUILD_DIR)/$(TARGET).elf verify reset exit"

# =============================================================================
# Clean
# =============================================================================
clean:
	rm -rf $(BUILD_DIR)

.PHONY: all flash clean