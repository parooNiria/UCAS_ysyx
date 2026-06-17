#include "vga_fb.h"

static uint32_t fb[kVgaFbSize];

extern "C" void vga_fb_write(int addr, int data) {
    uint32_t uaddr = (uint32_t)addr;
    if (uaddr < (uint32_t)kVgaFbSize)
        fb[uaddr] = (uint32_t)data;
}

extern "C" int vga_fb_read(int addr) {
    uint32_t uaddr = (uint32_t)addr;
    if (uaddr < (uint32_t)kVgaFbSize)
        return (int)fb[uaddr];
    return 0;
}

extern "C" int vga_fb_pixel(int x, int y) {
    if (x < 0 || x >= kVgaWidth || y < 0 || y >= kVgaHeight)
        return 0;
    uint32_t idx = (uint32_t)y * (uint32_t)kVgaWidth + (uint32_t)x;
    return (int)fb[idx];
}
