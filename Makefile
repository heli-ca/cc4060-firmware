# ==============================================================================
# CC4060 BLE Bridge Firmware Makefile (Linux Version)
# For JL AC6921A (pi32 architecture)
# 
# This Makefile is designed for Linux x86-64 environments (Ubuntu, CentOS, etc.)
# It will NOT work on macOS ARM64 or other incompatible architectures.
#
# Usage:
#   make                  - Build firmware
#   make clean            - Clean build artifacts
#
# Requirements:
#   - pi32-clang toolchain (automatically downloaded by build script)
#   - Python 3 (for BFU generation)
# ==============================================================================

# Toolchain configuration
TOOLCHAIN_PATH ?= $(shell pwd)/toolchain/pi32v2
CC = $(TOOLCHAIN_PATH)/bin/pi32-clang
LD = $(TOOLCHAIN_PATH)/bin/pi32-ld
OBJCOPY = $(TOOLCHAIN_PATH)/bin/pi32-objcopy

# Project configuration
TARGET = cc4060_ble_bridge
BUILD_DIR = build
SRC_DIR = src

# Compiler flags (from Jieli SDK)
CFLAGS = \
	-target pi32-unkown-unkown-unkown \
	-integrated-as \
	-fno-builtin \
	-mllvm -pi32-memreg-opt \
	-mllvm -pi32-mem-offset-adj-opt \
	-mllvm -pi32-const-spill \
	-mllvm -pi32-enable-jz \
	-mllvm -pi32-tailcall-opt \
	-mllvm -inline-threshold=5 \
	-mllvm -pi32-enable-itblock=1 \
	-nostrictpi32 \
	-Oz \
	-w

# Include paths
INCLUDES = \
	-I$(TOOLCHAIN_PATH)/include \
	-I$(TOOLCHAIN_PATH)/include/libc \
	-I$(SRC_DIR)

# Source files
SRCS = $(wildcard $(SRC_DIR)/*.c)

# Object files
OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))

# Linker flags
LDFLAGS = \
	-L$(TOOLCHAIN_PATH)/lib \
	-L$(TOOLCHAIN_PATH)/lib/r4-large

# Libraries
LIBS = \
	$(TOOLCHAIN_PATH)/lib/libcompiler-rt.a \
	$(TOOLCHAIN_PATH)/lib/r4-large/libc.a \
	$(TOOLCHAIN_PATH)/lib/r4-large/libm.a \
	$(TOOLCHAIN_PATH)/lib/r4-large/libg.a

# Output files
ELF = $(BUILD_DIR)/$(TARGET).app
BIN = $(BUILD_DIR)/jl_isd.bin
BFU = updata.bfu

# Colors for output
GREEN = \033[0;32m
YELLOW = \033[1;33m
RED = \033[0;31m
NC = \033[0m

# ==============================================================================
# Build targets
# ==============================================================================

.PHONY: all clean info help

all: $(BFU)
	@echo -e "$(GREEN)[SUCCESS]$(NC) Firmware build complete!"
	@echo -e "  BFU: $(BFU)"
	@ls -lh $(BFU) 2>/dev/null || true

# Create build directory
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

# Compile source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	@echo -e "$(YELLOW)[COMPILE]$(NC) $<"
	@$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Link object files
$(ELF): $(OBJS)
	@echo -e "$(YELLOW)[LINK]$(NC) Creating $(TARGET).app..."
	@$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)
	@echo -e "$(GREEN)[LINKED]$(NC) $(ELF)"

# Generate binary image
$(BIN): $(ELF)
	@echo -e "$(YELLOW)[OBJCOPY]$(NC) Generating binary..."
	@$(OBJCOPY) -O binary $< $@
	@echo -e "$(GREEN)[BINARY]$(NC) $@"

# Create BFU upgrade package
$(BFU): $(BIN)
	@echo -e "$(YELLOW)[BFU]$(NC) Creating upgrade package..."
	@python3 tools/bfu_builder.py build $(BIN) $(BFU)
	@echo -e "$(GREEN)[BFU]$(NC) $@ ($(shell stat -c%s $@ 2>/dev/null || stat -f%z $@ 2>/dev/null) bytes)"

# Clean build artifacts
clean:
	@echo -e "$(YELLOW)[CLEAN]$(NC) Removing build artifacts..."
	@rm -rf $(BUILD_DIR) $(BFU)
	@echo -e "$(GREEN)[CLEAN]$(NC) Done"

# Show build information
info:
	@echo -e "$(YELLOW)=== Build Information ===$(NC)"
	@echo -e "Toolchain:  $(TOOLCHAIN_PATH)"
	@echo -e "Compiler:   $(CC)"
	@echo -e "Target:     $(TARGET)"
	@echo -e "Sources:    $(SRCS)"
	@echo -e ""

# Help
help:
	@echo -e "$(YELLOW)=== CC4060 Firmware Build System ===$(NC)"
	@echo -e ""
	@echo -e "Targets:"
	@echo -e "  make          - Build firmware and generate BFU"
	@echo -e "  make clean    - Remove build artifacts"
	@echo -e "  make info     - Show build information"
	@echo -e "  make help     - Show this help"
	@echo -e ""
