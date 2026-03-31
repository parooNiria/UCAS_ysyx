TOPNAME    = top
VERILATOR ?= verilator

IMG_FILE_NAME ?= add-minirv-npc
BIN_BASE  ?= 0x80000000
PMEM_BASE ?= $(BIN_BASE)
PMEM_SIZE ?= 0x08000000
IMG       ?= /home/stu/ysyx-workbench/am-kernels/tests/cpu-tests/build/$(IMG_FILE_NAME).bin
MAX_CYCLES ?= 1000000

BUILD_DIR = $(abspath ./build)
OBJ_DIR   = $(BUILD_DIR)/obj_dir
BIN       = $(BUILD_DIR)/$(TOPNAME)

VSRCS = $(wildcard vsrc/*.v)
CSRCS = $(wildcard csrc/*.cpp)

VERILATOR_FLAGS = \
	--cc --exe --build \
	--top-module $(TOPNAME) \
	--trace-fst \
	-Mdir $(OBJ_DIR) \
	-GRESET_PC=$(BIN_BASE) \
	-CFLAGS "-std=c++17 -O2 -DPMEM_BASE=$(PMEM_BASE) -DPMEM_SIZE=$(PMEM_SIZE)" \
        -LDFLAGS "-lcapstone -ldl"
all: $(BIN)

$(BIN): $(VSRCS) $(CSRCS)
	@mkdir -p $(BUILD_DIR)
	$(VERILATOR) $(VERILATOR_FLAGS) $(VSRCS) $(CSRCS)
	@cp $(OBJ_DIR)/V$(TOPNAME) $(BIN)

run: $(BIN)
	@if [ -z "$(IMG)" ]; then \
		echo "Usage: make run IMG=/path/to/image.bin [BIN_BASE=0x80000000] [PMEM_BASE=0x80000000]"; \
		exit 1; \
	fi
	$(BIN) $(IMG)

sim: $(BIN)
	$(call git_commit, "sim RTL") # DO NOT REMOVE THIS LINE!!!
	@if [ -z "$(IMG)" ]; then \
		echo "Usage: make sim IMG=/path/to/image.bin [BIN_BASE=0x80000000] [PMEM_BASE=0x80000000]"; \
		exit 1; \
	fi
	$(BIN) $(IMG)

clean:
	@rm -rf $(BUILD_DIR) wave.fst

.PHONY: all run sim clean

include ../Makefile
