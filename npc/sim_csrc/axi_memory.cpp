#include "include/axi_memory.h"
#include "Vtop.h"
#include "include/memory.h"

void AxiLiteMemory::drive(Vtop *top) {
  top->io_axi_sram_awready = (!sram_write_pending && !sram_aw_captured) ? 1 : 0;
  top->io_axi_sram_wready = (!sram_write_pending && !sram_w_captured) ? 1 : 0;
  top->io_axi_sram_arready = sram_read_pending ? 0 : 1;
  top->io_axi_sram_rresp = 0;
  top->io_axi_sram_rvalid = sram_read_pending ? 1 : 0;
  top->io_axi_sram_rdata = sram_read_pending ? pmem_read(sram_read_addr) : 0;
  top->io_axi_sram_bresp = 0;
  top->io_axi_sram_bvalid = sram_write_pending ? 1 : 0;
}

void AxiLiteMemory::sample(Vtop *top) {
  if (!sram_read_pending && top->io_axi_sram_arvalid && top->io_axi_sram_arready) {
    sram_read_pending = true;
    sram_read_addr = top->io_axi_sram_araddr;
  }
  if (sram_read_pending && top->io_axi_sram_rvalid && top->io_axi_sram_rready) {
    sram_read_pending = false;
  }

  if (!sram_write_pending) {
    if (!sram_aw_captured && top->io_axi_sram_awvalid && top->io_axi_sram_awready) {
      sram_aw_captured = true;
      sram_aw_addr = top->io_axi_sram_awaddr;
    }
    if (!sram_w_captured && top->io_axi_sram_wvalid && top->io_axi_sram_wready) {
      sram_w_captured = true;
      sram_w_data = top->io_axi_sram_wdata;
      sram_w_strb = static_cast<unsigned char>(top->io_axi_sram_wstrb & 0x0fu);
    }

    if (sram_aw_captured && sram_w_captured) {
      pmem_write(static_cast<int>(sram_aw_addr), static_cast<int>(sram_w_data), static_cast<char>(sram_w_strb));
      sram_aw_captured = false;
      sram_w_captured = false;
      sram_write_pending = true;
    }
  }

  if (sram_write_pending && top->io_axi_sram_bvalid && top->io_axi_sram_bready) {
    sram_write_pending = false;
  }
}
