AM_SRCS := riscv/ysyxsoc/start.S \
           riscv/ysyxsoc/trm.c \
            riscv/ysyxsoc/ioe.c \
			riscv/ysyxsoc/timer.c \
			 riscv/ysyxsoc/cte.c \
			 platform/dummy/vme.c \
			 platform/dummy/mpe.c \
			 riscv/ysyxsoc/trap.S \
# 		    riscv/ysyxsoc/uart.c \
#            riscv/ysyxsoc/input.c \
           riscv/ysyxsoc/uart.c \
#            riscv/ysyxsoc/cte.c \
#            riscv/ysyxsoc/trap.S \
#            platform/dummy/vme.c \
#            platform/dummy/mpe.c

CFLAGS    += -fdata-sections -ffunction-sections
LDSCRIPTS += $(AM_HOME)/scripts/ysyxsoc-psram-run.ld
LDFLAGS   += 
LDFLAGS   += --gc-sections -e _start

MAINARGS_MAX_LEN = 64
MAINARGS_PLACEHOLDER = the_insert-arg_rule_in_Makefile_will_insert_mainargs_here
CFLAGS += -DMAINARGS_MAX_LEN=$(MAINARGS_MAX_LEN) -DMAINARGS_PLACEHOLDER=$(MAINARGS_PLACEHOLDER)

insert-arg: image
	@python $(AM_HOME)/tools/insert-arg.py $(IMAGE).bin $(MAINARGS_MAX_LEN) $(MAINARGS_PLACEHOLDER) "$(mainargs)"

image: image-dep
	@$(OBJDUMP) -D $(IMAGE).elf > $(IMAGE).txt
	@echo + OBJCOPY "->" $(IMAGE_REL).bin
	@$(OBJCOPY) -S --set-section-flags .bss=alloc,contents -O binary $(IMAGE).elf $(IMAGE).bin

run: insert-arg
		cd $(NPC_HOME) && make run IMG=$(IMAGE).bin

.PHONY: insert-arg