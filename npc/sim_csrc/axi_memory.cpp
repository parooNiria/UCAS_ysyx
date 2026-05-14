#include "include/axi_memory.h"
#include "Vtop.h"
#include "include/memory.h"
#include <stdlib.h>
#include <stdio.h>

namespace {

constexpr uint32_t kLegacyUartTxAddr = 0x10000000u;
constexpr uint32_t kUartBase = 0xA00003F8u;
constexpr uint32_t kClintBase = 0x02000000u;
constexpr uint32_t kClintCompatBase = 0xA0000048u;
constexpr uint32_t kClintMtimeLowOffset = 0xBFF8u;
constexpr uint32_t kClintMtimeHighOffset = 0xBFFCu;

bool is_uart_addr(uint32_t addr) {
  return addr == kLegacyUartTxAddr || (addr >= kUartBase && addr < kUartBase + 8);
}

bool is_clint_addr(uint32_t addr) {
  return (addr >= kClintBase && addr < kClintBase + 0x10000u) ||
         (addr >= kClintCompatBase && addr < kClintCompatBase + 8);
}

uint32_t read_uart(uint32_t addr) {
  if (addr == kLegacyUartTxAddr) {
    return 0;
  }
  uint32_t offset = addr - kUartBase;
  if (offset == 0) {
    return 0xFFFFFFFFu;
  }
  if (offset == 4) {
    return 0x00002000u;
  }
  return 0;
}

uint32_t read_clint(uint32_t addr, uint64_t mtime) {
  uint32_t offset = 0;
  if (addr >= kClintBase && addr < kClintBase + 0x10000u) {
    offset = addr - kClintBase;
  } else if (addr >= kClintCompatBase && addr < kClintCompatBase + 8) {
    offset = addr - kClintCompatBase;
  } else {
    return 0;
  }

  if (offset == kClintMtimeLowOffset) {
    return static_cast<uint32_t>(mtime & 0xFFFFFFFFu);
  }
  if (offset == kClintMtimeHighOffset) {
    return static_cast<uint32_t>(mtime >> 32);
  }
  if (addr >= kClintCompatBase && offset == 0) {
    return static_cast<uint32_t>(mtime & 0xFFFFFFFFu);
  }
  if (addr >= kClintCompatBase && offset == 4) {
    return static_cast<uint32_t>(mtime >> 32);
  }
  return 0;
}

void write_uart(uint32_t addr, uint32_t data) {
  if (addr == kLegacyUartTxAddr || addr == kUartBase) {
    putchar(static_cast<char>(data & 0xffu));
    fflush(stdout);
  }
}

bool should_use_mmio(uint32_t addr) {
  return is_uart_addr(addr) || is_clint_addr(addr);
}

} // namespace

void AxiLiteMemory::drive(Vtop *top) {
  top->io_master_awready = (!sram_write_pending && !sram_aw_captured) ? 1 : 0;
  top->io_master_wready = (!sram_write_pending && !sram_w_captured) ? 1 : 0;
  top->io_master_arready = sram_read_pending ? 0 : 1;
  top->io_master_rresp = 0;
  top->io_master_rvalid = (sram_read_pending && sram_read_delay_count == 0) ? 1 : 0;
  top->io_master_rlast = (sram_read_pending && sram_read_delay_count == 0) ? 1 : 0;
  top->io_master_rid = sram_read_id;
  top->io_master_rdata = (sram_read_pending && sram_read_delay_count == 0) ? sram_read_data : 0;
  top->io_master_bresp = 0;
  top->io_master_bid = sram_aw_id;
  top->io_master_bvalid = (sram_write_pending && sram_write_delay_count == 0) ? 1 : 0;
}

void AxiLiteMemory::sample(Vtop *top) {
  if (!sram_read_pending && top->io_master_arvalid && top->io_master_arready) {
    sram_read_pending = true;
    sram_read_addr = top->io_master_araddr;
    sram_read_id = top->io_master_arid;
    if (is_uart_addr(sram_read_addr)) {
      sram_read_data = read_uart(sram_read_addr);
    } else if (is_clint_addr(sram_read_addr)) {
      sram_read_data = read_clint(sram_read_addr, clint_mtime);
    } else {
      sram_read_data = pmem_read(sram_read_addr);
    }
    sram_read_delay_count = rand() % 5 + 1;
  }
  if (sram_read_pending) {
    if (sram_read_delay_count > 0) {
      sram_read_delay_count--;
    } else if (top->io_master_rvalid && top->io_master_rready && top->io_master_rlast) {
      sram_read_pending = false;
    }
  }

  if (!sram_write_pending) {
    if (!sram_aw_captured && top->io_master_awvalid && top->io_master_awready) {
      sram_aw_captured = true;
      sram_aw_addr = top->io_master_awaddr;
      sram_aw_id = top->io_master_awid;
    }
    if (!sram_w_captured && top->io_master_wvalid && top->io_master_wready) {
      sram_w_captured = true;
      sram_w_data = top->io_master_wdata;
      sram_w_strb = static_cast<unsigned char>(top->io_master_wstrb & 0x0fu);
    }

    if (sram_aw_captured && sram_w_captured) {
      if (is_uart_addr(sram_aw_addr)) {
        write_uart(sram_aw_addr, sram_w_data);
      } else if (is_clint_addr(sram_aw_addr)) {
        // CLINT is modeled as a free-running timer here; writes are ignored.
      } else {
        pmem_write(static_cast<int>(sram_aw_addr), static_cast<int>(sram_w_data), static_cast<char>(sram_w_strb));
      }
      sram_aw_captured = false;
      sram_w_captured = false;
      sram_write_pending = true;
      sram_write_delay_count = rand() % 5 + 1;
    }
  }

  if (sram_write_pending) {
    if (sram_write_delay_count > 0) {
      sram_write_delay_count--;
    } else if (top->io_master_bvalid && top->io_master_bready) {
      sram_write_pending = false;
    }
  }

  clint_mtime++;
}
