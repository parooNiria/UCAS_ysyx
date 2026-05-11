#ifndef __AXI_MEMORY_H__
#define __AXI_MEMORY_H__

class Vtop;

class AxiLiteMemory {
public:
  void drive(Vtop *top);
  void sample(Vtop *top);

private:
  bool sram_read_pending = false;
  unsigned sram_read_addr = 0;

  bool sram_aw_captured = false;
  unsigned sram_aw_addr = 0;
  bool sram_w_captured = false;
  unsigned sram_w_data = 0;
  unsigned char sram_w_strb = 0;

  bool sram_write_pending = false;
};

#endif
