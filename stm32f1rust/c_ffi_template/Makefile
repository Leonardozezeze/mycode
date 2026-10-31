# STM32F1 Rust 模板：一键构建并生成 .hex / .bin
#
#   make            # Debug：cargo build + hex/bin
#   make release    # Release：cargo build --release + hex/bin
#   make flash      # Release 构建并烧录
#   make clean      # 清理 cargo 和 CMake 产物

TARGET := thumbv7m-none-eabi
NAME   := c_ffi_template

CARGO   ?= cargo
OBJCOPY ?= arm-none-eabi-objcopy
SIZE    ?= arm-none-eabi-size
export SIZE

DEBUG_ELF   := target/$(TARGET)/debug/$(NAME)
RELEASE_ELF := target/$(TARGET)/release/$(NAME)

.PHONY: all debug release flash clean

all: debug

debug:
	$(CARGO) build
	$(OBJCOPY) -O ihex   $(DEBUG_ELF) $(DEBUG_ELF).hex
	$(OBJCOPY) -O binary $(DEBUG_ELF) $(DEBUG_ELF).bin
	bash scripts/size-report.sh $(DEBUG_ELF)

release:
	$(CARGO) build --release
	$(OBJCOPY) -O ihex   $(RELEASE_ELF) $(RELEASE_ELF).hex
	$(OBJCOPY) -O binary $(RELEASE_ELF) $(RELEASE_ELF).bin
	bash scripts/size-report.sh $(RELEASE_ELF)

# 烧录并运行（cargo 的 runner = probe-rs run --chip STM32F103C8）
flash:
	$(CARGO) run --release
	$(OBJCOPY) -O ihex   $(RELEASE_ELF) $(RELEASE_ELF).hex
	$(OBJCOPY) -O binary $(RELEASE_ELF) $(RELEASE_ELF).bin
	bash scripts/size-report.sh $(RELEASE_ELF)

clean:
	$(CARGO) clean
	rm -rf c_src/build
