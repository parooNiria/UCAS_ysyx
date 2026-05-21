CROSS_COMPILE = riscv64-linux-gnu-
CC = $(CROSS_COMPILE)gcc
OBJCOPY = $(CROSS_COMPILE)objcopy
OBJDUMP = $(CROSS_COMPILE)objdump

# 目标文件名
TARGET = char-test

# 编译与链接参数
# -march=rv32im_zicsr: 指定 RISC-V 指令集架构
# -mabi=ilp32: 指定应用二进制接口
# -nostdlib -ffreestanding -fno-builtin: 裸机环境，不依赖标准库
# -Wl,-Ttext=0x20000000: 指定代码段起始地址为 0x20000000
# -Wl,--entry=_start: 指定程序入口符号为 _start
CFLAGS = -march=rv32im_zicsr -mabi=ilp32 -nostdlib -ffreestanding -fno-builtin
LDFLAGS = -Wl,-Ttext=0x20000000 -Wl,--entry=_start

# 默认目标：生成 ELF 和 BIN 文件
all: $(TARGET).elf $(TARGET).bin

# 编译生成 ELF 文件
$(TARGET).elf: $(TARGET).c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

# 提取 .text 段并转换为纯二进制 BIN 文件
$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary -j .text $< $@

# 反汇编功能：将 ELF 文件反汇编为可读的 .S 文件
disasm: $(TARGET).elf
	$(OBJDUMP) -D $< > $(TARGET).S
	@echo "反汇编完成，已生成 $(TARGET).S"

run: $(TARGET).bin
	cd $(NPC_HOME) && make run IMG=/root/UCAS_ysyx/npc/test_csrc/$(TARGET).bin 

# 清理生成的中间文件和目标文件
clean:
	rm -f $(TARGET).elf $(TARGET).bin $(TARGET).S

# 声明伪目标，防止与实际文件重名产生冲突
.PHONY: all disasm clean