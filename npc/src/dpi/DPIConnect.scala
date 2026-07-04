package npc

import chisel3._
import chisel3.util._

// ===========================================================================
// DPIConnect — ALL DPI-C module instantiations and wiring in one place.
// Comment out the body of this module to exclude all DPI code for synthesis.
// ===========================================================================
class DPIConnect extends Module {
  val io = IO(new Bundle {
    // ── REFcommitDPI ──
    val commit_last    = Input(Bool())
    val dbg_rf         = Input(Vec(32, UInt(32.W)))
    val commit_mstatus = Input(UInt(32.W))
    val commit_mtvec   = Input(UInt(32.W))
    val commit_mepc    = Input(UInt(32.W))
    val commit_mcause  = Input(UInt(32.W))
    val last_inst      = Input(UInt(32.W))
    val next_pc        = Input(UInt(32.W))
    val last_pc        = Input(UInt(32.W))
    val device_access  = Input(Bool())

    // ── REFinitDPI ──
    val init_pulse     = Input(Bool())
    val init_mstatus   = Input(UInt(32.W))
    val init_mtvec     = Input(UInt(32.W))
    val init_mepc      = Input(UInt(32.W))
    val init_mcause    = Input(UInt(32.W))
    val init_pc        = Input(UInt(32.W))

    // ── EbreakDPI ──
    val ebreak         = Input(Bool())
    val ebreak_a0      = Input(UInt(32.W))

    // ── PerfEventDPI: IFU ──
    val ifu_fetch      = Input(Bool())

    // ── PerfEventDPI: IDU ──
    val idu_compute    = Input(Bool())
    val idu_branch     = Input(Bool())
    val idu_jump       = Input(Bool())
    val idu_load       = Input(Bool())
    val idu_store      = Input(Bool())
    val idu_csr        = Input(Bool())
    val idu_system     = Input(Bool())

    // ── PerfEventDPI: EXU ──
    val exu_compute     = Input(Bool())
    val exu_load_issue  = Input(Bool())
    val exu_store_issue = Input(Bool())

    // ── PerfEventDPI: LSU ──
    val lsu_load_done  = Input(Bool())
    val lsu_store_done = Input(Bool())

    // ── PerfEventDPI: WBU ──
    val wbu_commit     = Input(Bool())

    // ── PerfEventDPI: ICache ──
    val icache_access     = Input(Bool())
    val icache_hit        = Input(Bool())
    val icache_miss_cycle = Input(Bool())

    // ── PerfEventDPI: BTB ──
    val btb_lookup      = Input(Bool())
    val btb_hit         = Input(Bool())
    val btb_mispredict  = Input(Bool())

    // ── DtraceDPI: Data Trace ──
    val dtrace_load_valid  = Input(Bool())
    val dtrace_store_valid = Input(Bool())
    val dtrace_addr        = Input(UInt(32.W))
    val dtrace_mem_size    = Input(UInt(3.W))
    val dtrace_wdata       = Input(UInt(32.W))
    val dtrace_wstrb       = Input(UInt(4.W))
  })

  // =========================================================================
  //  COMMENT OUT BELOW to exclude all DPI for synthesis timing analysis
  // =========================================================================

  // ── REFcommitDPI (differential test commit) ──
  val commit_dpi = Module(new REFcommitDPI)
  commit_dpi.io.commit_enable := io.commit_last
  commit_dpi.io.dbg_rf        := io.dbg_rf
  commit_dpi.io.dbg_mstatus   := io.commit_mstatus
  commit_dpi.io.dbg_mtvec     := io.commit_mtvec
  commit_dpi.io.dbg_mepc      := io.commit_mepc
  commit_dpi.io.dbg_mcause    := io.commit_mcause
  commit_dpi.io.last_inst     := io.last_inst
  commit_dpi.io.next_pc       := io.next_pc
  commit_dpi.io.last_pc       := io.last_pc
  commit_dpi.io.device_type   := Cat(0.U(31.W), io.device_access)

  // ── REFinitDPI (differential test init) ──
  val init_dpi = Module(new REFinitDPI)
  init_dpi.io.init_enable := io.init_pulse
  init_dpi.io.dbg_mstatus := io.init_mstatus
  init_dpi.io.dbg_mtvec   := io.init_mtvec
  init_dpi.io.dbg_mepc    := io.init_mepc
  init_dpi.io.dbg_mcause  := io.init_mcause
  init_dpi.io.dbg_pc      := io.init_pc

  // ── EbreakDPI ──
  val ebreak_dpi = Module(new EbreakDPI)
  ebreak_dpi.io.enable     := io.ebreak
  ebreak_dpi.io.dbg_reg_a0 := io.ebreak_a0

  // ── PerfEventDPI ──
  val perf_dpi = Module(new PerfEventDPI)
  perf_dpi.io.clk := clock
  perf_dpi.io.ifu_fetch      := io.ifu_fetch
  perf_dpi.io.idu_compute    := io.idu_compute
  perf_dpi.io.idu_branch     := io.idu_branch
  perf_dpi.io.idu_jump       := io.idu_jump
  perf_dpi.io.idu_load       := io.idu_load
  perf_dpi.io.idu_store      := io.idu_store
  perf_dpi.io.idu_csr        := io.idu_csr
  perf_dpi.io.idu_system     := io.idu_system
  perf_dpi.io.exu_compute    := io.exu_compute
  perf_dpi.io.exu_load_issue := io.exu_load_issue
  perf_dpi.io.exu_store_issue:= io.exu_store_issue
  perf_dpi.io.lsu_load_done  := io.lsu_load_done
  perf_dpi.io.lsu_store_done := io.lsu_store_done
  perf_dpi.io.wbu_commit     := io.wbu_commit

  perf_dpi.io.icache_access     := io.icache_access
  perf_dpi.io.icache_hit        := io.icache_hit
  perf_dpi.io.icache_miss_cycle := io.icache_miss_cycle

  perf_dpi.io.btb_lookup      := io.btb_lookup
  perf_dpi.io.btb_hit         := io.btb_hit
  perf_dpi.io.btb_mispredict  := io.btb_mispredict

  // ── DtraceDPI ──
  val dtrace_dpi = Module(new DtraceDPI)
  dtrace_dpi.io.clk         := clock
  dtrace_dpi.io.load_valid  := io.dtrace_load_valid
  dtrace_dpi.io.store_valid := io.dtrace_store_valid
  dtrace_dpi.io.addr        := io.dtrace_addr
  dtrace_dpi.io.mem_size    := io.dtrace_mem_size
  dtrace_dpi.io.wdata       := io.dtrace_wdata
  dtrace_dpi.io.wstrb       := io.dtrace_wstrb

  // =========================================================================
  //  END OF DPI BLOCK
  // =========================================================================
}
