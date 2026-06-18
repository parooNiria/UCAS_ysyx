.DEFAULT_GOAL := all
TOPNAME    = ysyxSoCFull
VERILATOR ?= verilator
VERILATOR_ROOT := $(shell $(VERILATOR) --getenv VERILATOR_ROOT)
NVBOARD_HOME ?= $(abspath ../nvboard)
include $(NVBOARD_HOME)/scripts/nvboard.mk
NXDC_FILES = constr/ysyxsoc.nxdc

IMG_FILE_NAME ?= add-riscv32e-npc
BIN_BASE  ?= 0x80000000
PMEM_BASE ?= $(BIN_BASE)
PMEM_SIZE ?= 0x08000000
IMG       ?= $(NEMU_HOME)/../am-kernels/tests/cpu-tests/build/$(IMG_FILE_NAME).bin
MAX_CYCLES ?= 1000000
REF ?= spike

ifeq ($(REF), spike)
REF_SO_FILE ?= $(NEMU_HOME)/tools/spike-diff/build/riscv32-spike-so
else ifeq ($(REF), nemu)
REF_SO_FILE ?= $(NEMU_HOME)/build/riscv32-nemu-interpreter-so
else
$(error Unsupported REF value: $(REF). Supported values: spike, nemu)
endif

BUILD_DIR = $(abspath ./build)
OBJ_DIR   = $(BUILD_DIR)/obj_dir
BIN       = $(BUILD_DIR)/$(TOPNAME)
PERIP_DIR   := $(SOC_HOME)/perip
# 包含 C++ 仿真文件
CSRCS = $(wildcard soc_minimal/*.cpp )
# CSRCS = $(wildcard soc_csrc/*.cpp )
# 包含 Scala 源码和构建脚本，用于判断是否需要重新生成 Verilog
SCALA_SRCS = $(shell find $(abspath ./src) -name "*.scala" 2>/dev/null) $(wildcard build.sc build.mill)

VSRCS =  $$(find $(BUILD_DIR) -name "*.sv" -o -name "*.v" 2>/dev/null)
VSRCS += $(SOC_HOME)/build/ysyxSoCFull.v
VSRCS += $$(find $(PERIP_DIR) -name "*.sv" -o -name "*.v" 2>/dev/null)

# NVBoard include flags (INC_PATH, CXXFLAGS, LDFLAGS come from nvboard.mk)
NVBOARD_INCFLAGS = $(addprefix -I, $(INC_PATH))

VERILATOR_FLAGS = \
  --cc --exe --build -j 0 --trace-fst \
  --top-module $(TOPNAME) \
  -Mdir $(OBJ_DIR) \
  -I$(PERIP_DIR)/uart16550/rtl \
  -I$(PERIP_DIR)/spi/rtl \
  -I$(BUILD_DIR) \
  --timescale "1ns/1ns" \
  --no-timing \
  -CFLAGS "-std=c++17 -O3 $(NVBOARD_INCFLAGS) $(CXXFLAGS)" \
  -LDFLAGS "-lcapstone -ldl $(NVBOARD_ARCHIVE) $(LDFLAGS)" 
RUN_FLAGS = --no-limit  
all: $(BIN)

# NVBoard constraint → auto_bind.cpp
SRC_AUTO_BIND = $(abspath soc_csrc/auto_bind.cpp)
$(SRC_AUTO_BIND): $(NXDC_FILES)
	python3 $(NVBOARD_HOME)/scripts/auto_pin_bind.py $^ $@

test:
	COURSIER_REPOSITORIES="ivy2Local|https://maven.aliyun.com/repository/public" mill -i test

$(BUILD_DIR)/.verilog_stamp: $(SCALA_SRCS)
	$(call git_commit, "generate verilog")
	mkdir -p $(BUILD_DIR)
	COURSIER_REPOSITORIES="ivy2Local|https://maven.aliyun.com/repository/public" mill -i runMain Elaborate --target-dir $(BUILD_DIR)
	@touch $(BUILD_DIR)/.verilog_stamp

verilog: $(BUILD_DIR)/.verilog_stamp

$(BIN): $(BUILD_DIR)/.verilog_stamp $(CSRCS) $(NVBOARD_ARCHIVE)
	@mkdir -p $(BUILD_DIR)
	$(VERILATOR) $(VERILATOR_FLAGS) $(VSRCS) $(CSRCS)
	@rm -f $(BIN)
	@cp $(OBJ_DIR)/V$(TOPNAME) $(BIN)

run: $(BIN)
	@if [ -z "$(IMG)" ]; then \
		echo "Usage: make run IMG=/path/to/image.bin [BIN_BASE=0x80000000] [PMEM_BASE=0x80000000]"; \
		exit 1; \
	fi
	$(BIN) $(IMG) $(RUN_FLAGS)

sim: $(BIN)
	$(call git_commit, "sim RTL") # DO NOT REMOVE THIS LINE!!!
	@if [ -z "$(IMG)" ]; then \
		echo "Usage: make sim IMG=/path/to/image.bin [BIN_BASE=0x80000000] [PMEM_BASE=0x80000000]"; \
		exit 1; \
	fi
	$(BIN) $(IMG)

sim_file: $(BUILD_DIR)/.verilog_stamp $(CSRCS) $(NVBOARD_ARCHIVE)
	@mkdir -p $(BUILD_DIR)
	$(VERILATOR) $(VERILATOR_FLAGS) $(VSRCS) $(CSRCS)
	@rm -f $(BIN)
	@cp $(OBJ_DIR)/V$(TOPNAME) $(BIN)
help:
	mill -i runMain Elaborate --help

reformat:
	mill -i __.reformat

checkformat:
	mill -i __.checkFormat

bsp:
	mill -i mill.bsp.BSP/install

idea:
	mill -i mill.idea.GenIdea/idea

LINT_FLAGS = \
  --lint-only -Wall \
  --top-module $(TOPNAME) \
  -I$(PERIP_DIR)/uart16550/rtl \
  -I$(PERIP_DIR)/spi/rtl \
  -I$(BUILD_DIR) \
  --timescale "1ns/1ns" \
  --no-timing

lint: $(BUILD_DIR)/.verilog_stamp
	$(VERILATOR) $(LINT_FLAGS) $(VSRCS) > warning.txt 2>&1

clean:
	-rm -rf $(BUILD_DIR) wave.fst

.PHONY: all test verilog run sim help reformat checkformat bsp idea lint clean

-include ../Makefile