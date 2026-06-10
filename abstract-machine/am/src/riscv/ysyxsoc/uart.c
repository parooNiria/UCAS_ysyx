#include <am.h>
#include "ysyxsoc.h"

#define UART_LSR_PORT (SERIAL_PORT + 5)
#define UART_LSR_DR   0x01

void __am_uart_config(AM_UART_CONFIG_T *cfg) { cfg->present = true; }
void __am_uart_tx(AM_UART_TX_T *uart)        { outb(SERIAL_PORT, (uint8_t)uart->data); }

void __am_uart_rx(AM_UART_RX_T *uart) {
  uint8_t lsr = inb(UART_LSR_PORT);
  uart->data = (lsr & UART_LSR_DR) ? (char)inb(SERIAL_PORT) : (char)-1;
}
