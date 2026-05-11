TOPNAME    = top
VERILATOR ?= verilator
VERILATOR_ROOT := $(shell $(VERILATOR) --getenv VERILATOR_ROOT)

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

# 包含 C++ 仿真文件 (从当前的sim_csrc、csrc中捕获)
CSRCS = $(wildcard sim_csrc/*.cpp csrc/*.cpp sim_csrc/*.c)
# svdpi scope APIs (svGetScopeFromName/svSetScope) are implemented here in Verilator.
CSRCS += $(VERILATOR_ROOT)/include/verilated_dpi.cpp
# 包含 Scala 源码和构建脚本，用于判断是否需要重新生成 Verilog
SCALA_SRCS = $(shell find $(abspath ./src) -name "*.scala" 2>/dev/null) $(wildcard build.sc build.mill)

VERILATOR_FLAGS = \
	--cc --exe --build -j 0 --trace-fst \
	--top-module $(TOPNAME) \
 	-Mdir $(OBJ_DIR) \
 	-I$(BUILD_DIR) \
	-CFLAGS "-std=c++17 -O2 -DPMEM_BASE=$(PMEM_BASE) -DPMEM_SIZE=$(PMEM_SIZE)" \
	-LDFLAGS "-lcapstone -ldl"

all: $(BIN)

test:
	COURSIER_REPOSITORIES="ivy2Local|https://maven.aliyun.com/repository/public" mill -i test

$(BUILD_DIR)/.verilog_stamp: $(SCALA_SRCS)
	$(call git_commit, "generate verilog")
	mkdir -p $(BUILD_DIR)
	COURSIER_REPOSITORIES="ivy2Local|https://maven.aliyun.com/repository/public" mill -i runMain Elaborate --target-dir $(BUILD_DIR)
	@touch $(BUILD_DIR)/.verilog_stamp

verilog: $(BUILD_DIR)/.verilog_stamp

$(BIN): $(BUILD_DIR)/.verilog_stamp $(CSRCS)
	@mkdir -p $(BUILD_DIR)
	$(VERILATOR) $(VERILATOR_FLAGS) $$(find $(BUILD_DIR) -name "*.sv" -o -name "*.v" 2>/dev/null) $(wildcard vsrc/*.v) $(CSRCS)
	@rm -f $(BIN)
	@cp $(OBJ_DIR)/V$(TOPNAME) $(BIN)

run: $(BIN)
	@if [ -z "$(IMG)" ]; then \
		echo "Usage: make run IMG=/path/to/image.bin [BIN_BASE=0x80000000] [PMEM_BASE=0x80000000]"; \
		exit 1; \
	fi
	$(BIN) $(IMG) $(REF_SO_FILE) -s

sim: $(BIN)
	$(call git_commit, "sim RTL") # DO NOT REMOVE THIS LINE!!!
	@if [ -z "$(IMG)" ]; then \
		echo "Usage: make sim IMG=/path/to/image.bin [BIN_BASE=0x80000000] [PMEM_BASE=0x80000000]"; \
		exit 1; \
	fi
	$(BIN) $(IMG)

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

clean:
	-rm -rf $(BUILD_DIR) wave.fst

.PHONY: all test verilog run sim help reformat checkformat bsp idea clean

-include ../Makefile
