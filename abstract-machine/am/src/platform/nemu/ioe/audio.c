#include <am.h>
#include <nemu.h>
#include <klib.h>

#define AUDIO_FREQ_ADDR      (AUDIO_ADDR + 0x00)
#define AUDIO_CHANNELS_ADDR  (AUDIO_ADDR + 0x04)
#define AUDIO_SAMPLES_ADDR   (AUDIO_ADDR + 0x08)
#define AUDIO_SBUF_SIZE_ADDR (AUDIO_ADDR + 0x0c)
#define AUDIO_INIT_ADDR      (AUDIO_ADDR + 0x10)
#define AUDIO_COUNT_ADDR     (AUDIO_ADDR + 0x14)

void __am_audio_init() {
}

void __am_audio_config(AM_AUDIO_CONFIG_T *cfg) {
  cfg->present = true;
  cfg->bufsize = inl(AUDIO_SBUF_SIZE_ADDR);
}

void __am_audio_ctrl(AM_AUDIO_CTRL_T *ctrl) {
  outl(AUDIO_FREQ_ADDR, ctrl->freq);
  outl(AUDIO_CHANNELS_ADDR, ctrl->channels);
  outl(AUDIO_SAMPLES_ADDR, ctrl->samples);
  outl(AUDIO_INIT_ADDR, 1);
}

void __am_audio_status(AM_AUDIO_STATUS_T *stat) {
  stat->count = inl(AUDIO_COUNT_ADDR);
}

void __am_audio_play(AM_AUDIO_PLAY_T *ctl) {
  int len = ctl->buf.end - ctl->buf.start;
  int bufsize = inl(AUDIO_SBUF_SIZE_ADDR);
  uint8_t *sbuf = (uint8_t *)AUDIO_SBUF_ADDR;

  while (len > 0) {
    int count = inl(AUDIO_COUNT_ADDR);
    int free = bufsize - count;
    int nwrite = (len < free ? len : free);

    if (nwrite > 0) {
      int init = inl(AUDIO_INIT_ADDR);
      int write_pos = (init + count) % bufsize;
      
      int first_half = bufsize - write_pos;
      if (first_half >= nwrite) {
        memcpy(sbuf + write_pos, ctl->buf.start, nwrite);
      } else {
        memcpy(sbuf + write_pos, ctl->buf.start, first_half);
        memcpy(sbuf, (uint8_t *)ctl->buf.start + first_half, nwrite - first_half);
      }
      
      outl(AUDIO_COUNT_ADDR, count + nwrite);
      ctl->buf.start += nwrite;
      len -= nwrite;
    }
  }
}
