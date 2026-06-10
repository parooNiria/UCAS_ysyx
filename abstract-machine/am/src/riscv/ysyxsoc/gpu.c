#include <am.h>
#include "ysyxsoc.h"

void __am_gpu_config(AM_GPU_CONFIG_T *cfg) {
  cfg->present  = true;
  cfg->has_accel = false;
  cfg->width    = VGA_FB_W;
  cfg->height   = VGA_FB_H;
  cfg->vmemsz   = VGA_FB_W * VGA_FB_H * 4;
}

void __am_gpu_status(AM_GPU_STATUS_T *st) {
  st->ready = true;
}

void __am_gpu_fbdraw(AM_GPU_FBDRAW_T *ctl) {
  int x = ctl->x, y = ctl->y, w = ctl->w, h = ctl->h;
  uint32_t *pixels = (uint32_t *)ctl->pixels;
  for (int j = 0; j < h; j++) {
    for (int i = 0; i < w; i++) {
      uint32_t *dst = (uint32_t *)VGA_FB_BASE + (y + j) * VGA_FB_W + (x + i);
      *dst = pixels[j * w + i];
    }
  }
}

void __am_gpu_memcpy(AM_GPU_MEMCPY_T *ctl) {
  uint32_t  size = ctl->size;
  uint8_t  *src  = (uint8_t *)ctl->src;
  uint8_t  *dst  = (uint8_t *)(uintptr_t)ctl->dest;
  for (uint32_t i = 0; i < size; i++) dst[i] = src[i];
}
