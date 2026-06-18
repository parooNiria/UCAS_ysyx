package npc

import chisel3._
import chisel3.util._

class cpu extends Module {
  val io = IO(new Bundle {
    val axi_if = new AXI4Bundle
    val axi_mem = new AXI4Bundle
    val interrupt = Input(Bool())
  })

  val ifu = Module(new IFU)
  val idu = Module(new IDU)
  val exu = Module(new EXU)
  val memu = Module(new MEMU)
  val wbu = Module(new WBU)
  val rf = Module(new RegisterFile)
  val icache = Module(new ICache)  // I-Cache between IFU and AXI

  idu.io.in <> ifu.io.out
  exu.io.in <> idu.io.out
  memu.io.in <> exu.io.out
  wbu.io.in <> memu.io.out

  // ── Register file ──
  rf.io.raddr1 := idu.io.rf_read.raddr1
  rf.io.raddr2 := idu.io.rf_read.raddr2
  idu.io.rf_read.rdata1 := rf.io.rdata1
  idu.io.rf_read.rdata2 := rf.io.rdata2

  rf.io.wen := wbu.io.out.reg_we_en && wbu.io.out.commit_valid
  rf.io.waddr := wbu.io.out.reg_dest
  rf.io.wdata := wbu.io.out.reg_write_data

  // ── Flush routing ──
  // IDU branch flush (br_taken, JAL, JALR) → earliest redirect to IFU
  // WBU exception flush (ecall, ebreak, mret, fencei, inv_inst) → full pipeline flush
  ifu.io.flush_valid := idu.io.flush_valid_out || wbu.io.out.flush_valid || exu.io.exp_status || memu.io.exp_status
  ifu.io.flush_pc     := Mux(wbu.io.out.flush_valid, wbu.io.out.flush_pc, idu.io.flush_re_pc)
  idu.io.flush_valid_in := wbu.io.out.flush_valid || exu.io.exp_status || memu.io.exp_status

  // ── Data forwarding ──
  idu.io.reg_forward_exe <> exu.io.reg_forward
  idu.io.reg_forward_mem <> memu.io.reg_forward
  idu.io.reg_forward_wb  <> wbu.io.reg_forward

  // IFU → ICache → AXI
  icache.io.if_req <> ifu.io.if_sram
  icache.io.fencei_req := wbu.io.out.fencei
  io.axi_if <> icache.io.axi

  io.axi_mem.awaddr := exu.io.awaddr
  io.axi_mem.awvalid := exu.io.awvalid
  exu.io.awready := io.axi_mem.awready
  io.axi_mem.awid := exu.io.awid
  io.axi_mem.awlen := exu.io.awlen
  io.axi_mem.awsize := exu.io.awsize
  io.axi_mem.awburst := exu.io.awburst

  io.axi_mem.wdata := exu.io.wdata
  io.axi_mem.wstrb := exu.io.wstrb
  io.axi_mem.wvalid := exu.io.wvalid
  exu.io.wready := io.axi_mem.wready
  io.axi_mem.wlast := exu.io.wlast

  io.axi_mem.araddr := exu.io.araddr
  io.axi_mem.arvalid := exu.io.arvalid
  exu.io.arready := io.axi_mem.arready
  io.axi_mem.arid := exu.io.arid
  io.axi_mem.arlen := exu.io.arlen
  io.axi_mem.arsize := exu.io.arsize
  io.axi_mem.arburst := exu.io.arburst

  memu.io.rdata := io.axi_mem.rdata
  memu.io.rresp := io.axi_mem.rresp
  memu.io.rvalid := io.axi_mem.rvalid
  memu.io.rlast := io.axi_mem.rlast
  memu.io.rid := io.axi_mem.rid
  io.axi_mem.rready := memu.io.rready

  memu.io.bresp := io.axi_mem.bresp
  memu.io.bvalid := io.axi_mem.bvalid
  memu.io.bid := io.axi_mem.bid
  io.axi_mem.bready := memu.io.bready

  // Commit tracking registers (for difftest DPI)
  val commit_last = RegInit(false.B)
  val last_inst = Reg(UInt(32.W))
  val last_pc = Reg(UInt(32.W))
  val last_next_pc = Reg(UInt(32.W))
  when(wbu.io.out.commit_valid) {
    commit_last := true.B
    last_inst := wbu.io.out.inst
    last_pc := wbu.io.out.pc
    last_next_pc := wbu.io.out.next_pc
  } .otherwise {
    commit_last := false.B
    last_inst := 0.U
    last_pc := 0.U
    last_next_pc := 0.U
  }

  // Reset edge detection (for difftest init DPI)
  val rst = this.reset.asBool
  val rst_delayed = RegNext(rst)
  val init_pulse = rst_delayed && !rst

  // =========================================================================
  // DPIConnect — all DPI-C modules in one place.
  // Comment out these lines to exclude all DPI for synthesis timing analysis.
  // =========================================================================
  val dpi = Module(new DPIConnect)

  // REFcommitDPI
  dpi.io.commit_last    := commit_last
  dpi.io.dbg_rf         := rf.io.rf_dbg
  dpi.io.commit_mstatus := wbu.io.dbg_mstatus
  dpi.io.commit_mtvec   := wbu.io.dbg_mtvec
  dpi.io.commit_mepc    := wbu.io.dbg_mepc
  dpi.io.commit_mcause  := wbu.io.dbg_mcause
  dpi.io.last_inst      := last_inst
  dpi.io.next_pc        := last_next_pc
  dpi.io.last_pc        := last_pc
  dpi.io.device_access  := wbu.io.out.device_access

  // REFinitDPI
  dpi.io.init_pulse     := init_pulse
  dpi.io.init_mstatus   := wbu.io.dbg_mstatus
  dpi.io.init_mtvec     := wbu.io.dbg_mtvec
  dpi.io.init_mepc      := wbu.io.dbg_mepc
  dpi.io.init_mcause    := wbu.io.dbg_mcause
  dpi.io.init_pc        := ifu.io.out.bits.pc

  // EbreakDPI
  dpi.io.ebreak         := wbu.io.out.ebreak
  dpi.io.ebreak_a0      := rf.io.rf_dbg(10)

  // PerfEventDPI: IFU
  dpi.io.ifu_fetch      := ifu.io.out.valid && ifu.io.out.ready

  // PerfEventDPI: IDU
  dpi.io.idu_compute    := idu.io.perf_events.compute
  dpi.io.idu_branch     := idu.io.perf_events.branch
  dpi.io.idu_jump       := idu.io.perf_events.jump
  dpi.io.idu_load       := idu.io.perf_events.load
  dpi.io.idu_store      := idu.io.perf_events.store
  dpi.io.idu_csr        := idu.io.perf_events.csr
  dpi.io.idu_system     := idu.io.perf_events.system

  // PerfEventDPI: EXU
  dpi.io.exu_compute    := exu.io.perf_compute
  dpi.io.exu_load_issue := exu.io.perf_load_issue
  dpi.io.exu_store_issue:= exu.io.perf_store_issue

  // PerfEventDPI: LSU
  dpi.io.lsu_load_done  := memu.io.perf_load
  dpi.io.lsu_store_done := memu.io.perf_store

  // PerfEventDPI: WBU
  dpi.io.wbu_commit     := wbu.io.out.commit_valid

  // PerfEventDPI: ICache
  dpi.io.icache_access     := icache.io.perf.access
  dpi.io.icache_hit        := icache.io.perf.hit
  dpi.io.icache_miss_cycle := icache.io.perf.miss_cycle
  // =========================================================================
  // END OF DPI BLOCK
  // =========================================================================
}
