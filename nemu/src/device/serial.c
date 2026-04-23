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

#include <utils.h>
#include <device/map.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h>

/* http://en.wikibooks.org/wiki/Serial_Programming/8250_UART_Programming */
// NOTE: this is compatible to 16550

#define CH_OFFSET 0
#define LSR_OFFSET 5

#define LSR_DATA_READY 0x01
#define LSR_TX_READY   0x20

#define SERIAL_FIFO_PATH "/tmp/nemu.serial"

static uint8_t *serial_base = NULL;
static int serial_input_fd = -1;
static int serial_rx_pending = -1;

static void serial_open_input() {
#ifdef CONFIG_SERIAL_INPUT_FIFO
  int ret = mkfifo(SERIAL_FIFO_PATH, 0666);
  if (ret != 0 && errno != EEXIST) {
    panic("failed to create %s: %s", SERIAL_FIFO_PATH, strerror(errno));
  }
  serial_input_fd = open(SERIAL_FIFO_PATH, O_RDWR | O_NONBLOCK, 0);
  if (serial_input_fd < 0) {
    panic("failed to open %s: %s", SERIAL_FIFO_PATH, strerror(errno));
  }
  Log("serial input fifo enabled: %s", SERIAL_FIFO_PATH);
#else
  serial_input_fd = STDIN_FILENO;
  int flags = fcntl(serial_input_fd, F_GETFL, 0);
  if (flags < 0) {
    panic("failed to get stdin flags: %s", strerror(errno));
  }
  if (fcntl(serial_input_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
    panic("failed to set stdin nonblock: %s", strerror(errno));
  }
#endif
}

static void serial_try_recv_byte() {
  if (serial_rx_pending >= 0 || serial_input_fd < 0) {
    return;
  }

  uint8_t ch = 0;
  ssize_t n = read(serial_input_fd, &ch, 1);
  if (n == 1) {
    serial_rx_pending = ch;
    return;
  }

  if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
    panic("serial input read failed: %s", strerror(errno));
  }
}

static uint8_t serial_lsr_value() {
  return LSR_TX_READY | (serial_rx_pending >= 0 ? LSR_DATA_READY : 0);
}


static void serial_putc(char ch) {
  MUXDEF(CONFIG_TARGET_AM, putch(ch), putc(ch, stderr));
}

static void serial_io_handler(uint32_t offset, int len, bool is_write) {
  assert(len == 1);
  switch (offset) {
    /* We bind the serial port with the host stderr in NEMU. */
    case CH_OFFSET:
      if (is_write) serial_putc(serial_base[0]);
      else {
        serial_try_recv_byte();
        if (serial_rx_pending >= 0) {
          serial_base[CH_OFFSET] = (uint8_t)serial_rx_pending;
          serial_rx_pending = -1;
        } else {
          serial_base[CH_OFFSET] = 0xff;
        }
      }
      break;
    case LSR_OFFSET:
      if (!is_write) {
        serial_try_recv_byte();
        serial_base[LSR_OFFSET] = serial_lsr_value();
      }
      break;
    default:
      if (!is_write) {
        serial_base[offset] = 0;
      }
      break;
  }
}

void init_serial() {
  serial_base = new_space(8);
  memset(serial_base, 0, 8);
  serial_base[LSR_OFFSET] = LSR_TX_READY;
  serial_open_input();
#ifdef CONFIG_HAS_PORT_IO
  add_pio_map ("serial", CONFIG_SERIAL_PORT, serial_base, 8, serial_io_handler);
#else
  add_mmio_map("serial", CONFIG_SERIAL_MMIO, serial_base, 8, serial_io_handler);
#endif

}
