#include <am.h>
#include "npc.h"

#define UART_LSR_PORT (SERIAL_PORT + 5)
#define UART_LSR_DR   0x01

void __am_uart_config(AM_UART_CONFIG_T *cfg) {
  cfg->present = true;
}

void __am_uart_tx(AM_UART_TX_T *uart) {
  outb(SERIAL_PORT, (uint8_t)uart->data);
}

void __am_uart_rx(AM_UART_RX_T *uart) {
  uint8_t lsr = inb(UART_LSR_PORT);
  if (lsr & UART_LSR_DR) {
    uart->data = (char)inb(SERIAL_PORT);
  } else {
    uart->data = (char)-1;
  }
}