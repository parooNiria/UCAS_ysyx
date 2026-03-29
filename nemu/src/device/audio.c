/***************************************************************************************
* Copyright (c) 2014-2024 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include <common.h>
#include <device/map.h>
#include <SDL2/SDL.h>

enum {
  reg_freq,
  reg_channels,
  reg_samples,
  reg_sbuf_size,
  reg_init,
  reg_count,
  nr_reg
};

static uint8_t *sbuf = NULL;
static uint32_t *audio_base = NULL;

static void audio_play_cb(void *userdata, uint8_t *stream, int len) {
  int nread = len;
  if (audio_base[reg_count] < len) nread = audio_base[reg_count];

  int first_half = CONFIG_SB_SIZE - audio_base[reg_init];
  if (first_half >= nread) {
    memcpy(stream, sbuf + audio_base[reg_init], nread);
    audio_base[reg_init] = (audio_base[reg_init] + nread) % CONFIG_SB_SIZE;
  } else {
    memcpy(stream, sbuf + audio_base[reg_init], first_half);
    memcpy(stream + first_half, sbuf, nread - first_half);
    audio_base[reg_init] = nread - first_half;
  }

  audio_base[reg_count] -= nread;
  if (len > nread) memset(stream + nread, 0, len - nread);
}

static void audio_io_handler(uint32_t offset, int len, bool is_write) {
  if (is_write && offset == reg_init * 4) {
    if (audio_base[reg_init] == 1) {
      SDL_AudioSpec spec;
      spec.freq = audio_base[reg_freq];
      spec.format = AUDIO_S16SYS;
      spec.channels = audio_base[reg_channels];
      spec.samples = audio_base[reg_samples];
      spec.callback = audio_play_cb;
      spec.userdata = NULL;

      SDL_InitSubSystem(SDL_INIT_AUDIO);
      SDL_OpenAudio(&spec, NULL);
      SDL_PauseAudio(0);
      audio_base[reg_init] = 0; // reset to 0 to act as read pointer
    }
  }
}

void init_audio() {
  uint32_t space_size = sizeof(uint32_t) * nr_reg;
  audio_base = (uint32_t *)new_space(space_size);
  audio_base[reg_sbuf_size] = CONFIG_SB_SIZE;
  audio_base[reg_init] = 0;
  audio_base[reg_count] = 0;
#ifdef CONFIG_HAS_PORT_IO
  add_pio_map ("audio", CONFIG_AUDIO_CTL_PORT, audio_base, space_size, audio_io_handler);
#else
  add_mmio_map("audio", CONFIG_AUDIO_CTL_MMIO, audio_base, space_size, audio_io_handler);
#endif

  sbuf = (uint8_t *)new_space(CONFIG_SB_SIZE);
  add_mmio_map("audio-sbuf", CONFIG_SB_ADDR, sbuf, CONFIG_SB_SIZE, NULL);
}
