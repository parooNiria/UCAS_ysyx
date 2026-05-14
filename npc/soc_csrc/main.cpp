#include <verilated.h>
#include "VysyxSoCFull.h"
#include <assert.h>
#include <stdint.h>
int g_ebreak = false;
// 桩函数，避免链接错误
extern "C" void flash_read(int32_t addr, int32_t *data) { *data = 0; }
extern "C" void mrom_read(int32_t addr, int32_t *data) { *data = 0x00100073; }

extern "C" void dpi_ebreak() {
    g_ebreak = true;
    printf("DPI ebreak called!\n");
}



int main(int argc, char** argv) {
    VerilatedContext* contextp = new VerilatedContext;
    contextp->commandArgs(argc, argv);
    
    // 初始化主模块
    VysyxSoCFull* top = new VysyxSoCFull(contextp);
    contextp->traceEverOn(true);
    top->clock = 0;
    top->reset = 1;

    // 复位几个周期
    for (int i = 0; i < 10; ++i) {
        top->clock = 0;
        top->eval();
        contextp->timeInc(1);
        
        top->clock = 1;
        top->eval();
        contextp->timeInc(1);
    }

    top->reset = 0;

    // 运行仿真
    while (!contextp->gotFinish() && contextp->time() < 1000000) {
        top->clock = 0;
        top->eval();
        contextp->timeInc(1);
        
        top->clock = 1;
        top->eval();
        contextp->timeInc(1);
        if (g_ebreak) {
            printf("Ebreak detected at time %lu\n", contextp->time());
            break;
        }
    }

    top->final();
    delete top;
    delete contextp;

    return 0;
}
