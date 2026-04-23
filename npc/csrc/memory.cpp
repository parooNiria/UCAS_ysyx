#include "include/memory.h"
#include "include/trace.h"

const uint32_t kPmemBase = static_cast<uint32_t>(PMEM_BASE);
const uint32_t kPmemSize = static_cast<uint32_t>(PMEM_SIZE);
static uint8_t pmem_arr[kPmemSize] = {0};
uint8_t *pmem = pmem_arr;

static constexpr uint32_t kSerialPort = 0xa00003f8u;
static constexpr uint32_t kSerialWord0 = kSerialPort & ~0x3u;
static constexpr uint32_t kSerialWord1 = (kSerialPort + 4u) & ~0x3u;
static constexpr uint8_t kUartLsrTxReady = 0x20u;

static inline bool is_serial_word(uint32_t addr) {
  return addr == kSerialWord0 || addr == kSerialWord1;
}

static inline uint32_t serial_mmio_read(uint32_t addr) {
  if (addr == kSerialWord0) {
    // No RX data ready; return 0xff for byte reads.
    return 0xffffffffu;
  }
  // LSR is at SERIAL_PORT + 5 -> byte lane 1 of 0xa00003fc.
  return static_cast<uint32_t>(kUartLsrTxReady) << 8;
}

static inline void serial_mmio_write(uint32_t addr, uint32_t data, uint8_t mask) {
  if (addr == kSerialWord0 && (mask & 0x01u)) {
    putchar(static_cast<int>(data & 0xffu));
    fflush(stdout);
  }
}

bool in_pmem(uint32_t addr) {
  return addr >= kPmemBase &&
         (static_cast<uint64_t>(addr) + 3) < (static_cast<uint64_t>(kPmemBase) + kPmemSize);
}

int pmem_read_internal(uint32_t addr) {
  if (!in_pmem(addr)) {
    log_mtrace_err("pmem_read_internal", addr);
    return -1;
  }
  uint32_t offset = addr - kPmemBase;
  int data = 0;
  data |= pmem[offset+0] << 0;
  data |= pmem[offset+1] << 8;
  data |= pmem[offset+2] << 16;
  data |= pmem[offset+3] << 24;
  return data;
}

int pmem_read(int raddr) {
  uint32_t addr = static_cast<uint32_t>(raddr) & ~0x3u;
  if (is_serial_word(addr)) {
    int data = static_cast<int>(serial_mmio_read(addr));
    log_mtrace_read(addr, data);
    return data;
  }
  if (addr < kPmemBase || addr >= kPmemBase + kPmemSize) {
    log_mtrace_err("pmem_read", addr);
    g_mem_assert_fail = true;
    return -1;
  }
  uint32_t offset = addr - kPmemBase;
  int data = 0;
  data |= pmem[offset+0] << 0;
  data |= pmem[offset+1] << 8;
  data |= pmem[offset+2] << 16;
  data |= pmem[offset+3] << 24;
  log_mtrace_read(addr, data);
  return data;
}

extern "C" void pmem_write(int waddr, int wdata, char wmask) {
  uint32_t addr = static_cast<uint32_t>(waddr) & ~0x3u;
  uint8_t mask = static_cast<uint8_t>(wmask);

  if (is_serial_word(addr)) {
    uint32_t data = static_cast<uint32_t>(wdata);
    serial_mmio_write(addr, data, mask);
    log_mtrace_write(addr, data, mask);
    return;
  }
  
  if (!in_pmem(addr)) {
    printf("[ERROR] pmem_write addr=0x%08x out of range\n", addr);
    log_mtrace_err("pmem_write", addr);
    g_mem_assert_fail = true;
    return;
  }
  uint32_t offset = addr - kPmemBase;
  uint32_t data = static_cast<uint32_t>(wdata);
  for (int i = 0; i < 4; i++) {
    if ((mask >> i) & 1) {
      pmem[offset+i] = (data >> (i*8)) & 0xff;
    }
  }
  log_mtrace_write(addr, data, mask);
}

void scan_memory(uint32_t addr, int len) {
  for (int i = 0; i < len; i++) {
    if (i % 4 == 0) printf("\n0x%08x: ", addr + i * 4);
    printf("0x%08x ", pmem_read_internal(addr + i * 4));
  }
  printf("\n");
}

long load_img(const char *img_file) {
  if (!img_file) return 0;
  FILE *fp = fopen(img_file, "rb");
  if (!fp) { perror("fopen failed"); exit(1); }

  fseek(fp, 0, SEEK_END);
  long size = ftell(fp);
  rewind(fp);
  size_t ret = fread(pmem, 1, size, fp);
  (void)ret;
  fclose(fp);
  printf("Load image: %s, size=%ld, base=0x%08x\n", img_file, size, kPmemBase);
  return size;
}
