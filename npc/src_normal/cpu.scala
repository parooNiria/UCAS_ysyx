package npc

import chisel3._
import chisel3.util._

// ===========================================================================
// CPU without DPI-C (except ebreak) — for synthesis / timing analysis.
// Only EbreakDPI is kept for simulation control; all other DPI removed.
// ===========================================================================
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
  val btb = Module(new BTB)        // Branch Target Buffer

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

  // ── BTB (Branch Target Buffer) ──
  // Lookup: 用当前取指 PC 查 BTB
  btb.io.lookup_pc := ifu.io.if_sram.addr
  // 结果送给 IFU
  ifu.io.btb_hit    := btb.io.hit
  ifu.io.btb_target := btb.io.pred_target

  // Update: IDU 决定是否写入 (实际跳转的分支才写)
  btb.io.update_valid  := idu.io.btb_update
  btb.io.update_pc     := idu.io.out.bits.pc
  btb.io.update_target := idu.io.flush_re_pc
  btb.io.update_taken  := true.B

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

  // ── EbreakDPI (simulation control only, not for synthesis) ──
  val ebreak_dpi = Module(new EbreakDPI)
  ebreak_dpi.io.enable     := wbu.io.out.ebreak
  ebreak_dpi.io.dbg_reg_a0 := rf.io.rf_dbg(10)
}
