#ifndef VGA_FB_H
#define VGA_FB_H

#include <cstdint>

static const int kVgaWidth  = 640;
static const int kVgaHeight = 480;
static const int kVgaFbSize = kVgaWidth * kVgaHeight;  // 307200

#ifdef __cplusplus
extern "C" {
#endif

void vga_fb_write(int addr, int data);
int  vga_fb_read (int addr);
int  vga_fb_pixel(int x, int y);

#ifdef __cplusplus
}
#endif

#endif
