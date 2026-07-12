# =============================================================================
# Telematics Gateway - Explicit Build (KOAS Strategy)
# Target    : STM32F446RET6 (NUCLEO-F446RE)
# =============================================================================

TARGET    = telematics_gateway
BUILD_DIR = build

# --- Toolchain ---
PREFIX  = arm-none-eabi-
CC      = $(PREFIX)gcc
AS      = $(PREFIX)gcc
OBJCOPY = $(PREFIX)objcopy
SIZE    = $(PREFIX)size

# --- Architecture flags ---
ARCH_FLAGS = -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard

# --- Include paths ---
INCLUDES = -Iapp/inc \
           -Imiddleware/event_queue/inc \
           -Imiddleware/protocol/inc \
           -Idrivers/peripheral/uart \
           -Idrivers/peripheral/dma/inc \
           -Iplatform/inc \
           -Ithird_party/cmsis/Core/Include \
           -Ithird_party/cmsis/Device/STSTM32F4xx/Include

# --- Compiler flags ---
CFLAGS = $(ARCH_FLAGS) -DSTM32F446xx -DDEBUG $(INCLUDES) \
         -Wall -Wextra -std=c11 -O0 -g3 -ffunction-sections -fdata-sections

# --- Linker flags ---
LDFLAGS = $(ARCH_FLAGS) -Tlinker/STM32F446RETX_FLASH.ld \
          -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--gc-sections \
          --specs=nano.specs --specs=nosys.specs -lc -lm

# --- Object Files ---
C_OBJS = $(BUILD_DIR)/main.o \
         $(BUILD_DIR)/event_queue.o \
         $(BUILD_DIR)/nmea_parser.o \
         $(BUILD_DIR)/uart_driver.o \
         $(BUILD_DIR)/dma_driver.o \
         $(BUILD_DIR)/system_stm32f4xx.o

ASM_OBJS = $(BUILD_DIR)/startup.o

# =============================================================================
# Build Targets
# =============================================================================

all: $(BUILD_DIR) $(BUILD_DIR)/$(TARGET).bin size

$(BUILD_DIR):
	mkdir $(BUILD_DIR)

# -----------------------------------------------------------------------------
# Explicit C compilation rules (The KOAS Strategy)
# -----------------------------------------------------------------------------
$(BUILD_DIR)/main.o: app/src/main.c
	@echo "  CC  app/src/main.c"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/event_queue.o: middleware/event_queue/src/event_queue.c
	@echo "  CC  middleware/event_queue/src/event_queue.c"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/nmea_parser.o: middleware/protocol/src/nmea_parser.c
	@echo "  CC  middleware/protocol/src/nmea_parser.c"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/uart_driver.o: drivers/peripheral/uart/uart_driver.c
	@echo "  CC  drivers/peripheral/uart/uart_driver.c"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/dma_driver.o: drivers/peripheral/dma/src/dma_driver.c
	@echo "  CC  drivers/peripheral/dma/src/dma_driver.c"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/system_stm32f4xx.o: platform/src/system_stm32f4xx.c
	@echo "  CC  platform/src/system_stm32f4xx.c"
	$(CC) $(CFLAGS) -c $< -o $@

# -----------------------------------------------------------------------------
# Explicit Assembly compilation rule
# -----------------------------------------------------------------------------
$(BUILD_DIR)/startup.o: startup/startup_stm32f446xx.s
	@echo "  AS  startup/startup_stm32f446xx.s"
	$(AS) $(ARCH_FLAGS) -x assembler-with-cpp -c $< -o $@

# -----------------------------------------------------------------------------
# Link & Binary
# -----------------------------------------------------------------------------
$(BUILD_DIR)/$(TARGET).elf: $(C_OBJS) $(ASM_OBJS)
	@echo "  LD  $@"
	$(CC) $(LDFLAGS) $^ -o $@

$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@

size: $(BUILD_DIR)/$(TARGET).elf
	$(SIZE) $<

# =============================================================================
# Utility Targets
# =============================================================================
clean:
	rm -rf $(BUILD_DIR)

flash: $(BUILD_DIR)/$(TARGET).elf
	openocd -f interface/stlink.cfg -f target/stm32f4x.cfg -c "program $< verify reset exit"

.PHONY: all clean flash size