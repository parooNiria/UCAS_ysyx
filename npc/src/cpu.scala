package npc

import chisel3._
import chisel3.util._

class cpu extends Module {
  val io = IO(new Bundle {
    val axi_if = new AXI4Bundle
    val axi_mem = new AXI4Bundle
    val interrupt = Input(Bool())

    val pc = Output(UInt(32.W))
    val inst = Output(UInt(32.W))
    val ebreak = Output(Bool())
    val commit_valid = Output(Bool())
    val device_access = Output(Bool())
    val dbg_rf      = Output(Vec(32, UInt(32.W)))
    val dbg_mstatus = Output(UInt(32.W))
    val dbg_mtvec   = Output(UInt(32.W))
    val dbg_mepc    = Output(UInt(32.W))
    val dbg_mcause  = Output(UInt(32.W))
  })

  val ifu = Module(new IFU)
  val idu = Module(new IDU)
  val exu = Module(new EXU)
  val memu = Module(new MEMU)
  val wbu = Module(new WBU)
  val rf = Module(new RegisterFile)

  ifu.io.commit_info.commit_valid := wbu.io.out.commit_valid
  ifu.io.commit_info.next_pc := wbu.io.out.next_pc

  idu.io.in <> ifu.io.out
  exu.io.in <> idu.io.out
  memu.io.in <> exu.io.out
  wbu.io.in <> memu.io.out

  rf.io.raddr1 := idu.io.rf_read.raddr1
  rf.io.raddr2 := idu.io.rf_read.raddr2
  idu.io.rf_read.rdata1 := rf.io.rdata1
  idu.io.rf_read.rdata2 := rf.io.rdata2

  rf.io.wen := wbu.io.out.reg_we_en && wbu.io.out.commit_valid
  rf.io.waddr := wbu.io.out.reg_dest
  rf.io.wdata := wbu.io.out.reg_write_data

  io.axi_if <> ifu.io.if_axi

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

  io.pc := wbu.io.out.next_pc
  io.inst := wbu.io.out.inst
  io.ebreak := wbu.io.out.ebreak
  io.commit_valid := wbu.io.out.commit_valid
  io.device_access := wbu.io.out.device_access
  io.dbg_rf := rf.io.rf_dbg
  io.dbg_mstatus := wbu.io.dbg_mstatus
  io.dbg_mtvec := wbu.io.dbg_mtvec
  io.dbg_mepc := wbu.io.dbg_mepc
  io.dbg_mcause := wbu.io.dbg_mcause
}
