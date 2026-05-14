package npc

import chisel3._
import chisel3.util._

class ysyx_00000000 extends Module {
  val io = IO(new Bundle {
    val interrupt = Input(Bool())
    val master = new AXI4Bundle
    val slave = Flipped(new AXI4Bundle)

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

  val cpu = Module(new cpu)
  val Arbiter = Module(new AXI4Arbiter)

  // Connect dbg signals
  io.pc := cpu.io.pc
  io.inst := cpu.io.inst
  io.ebreak := cpu.io.ebreak
  io.commit_valid := cpu.io.commit_valid
  io.device_access := cpu.io.device_access
  io.dbg_rf := cpu.io.dbg_rf
  io.dbg_mstatus := cpu.io.dbg_mstatus
  io.dbg_mtvec := cpu.io.dbg_mtvec
  io.dbg_mepc := cpu.io.dbg_mepc
  io.dbg_mcause := cpu.io.dbg_mcause

  // Connect Arbiter
  Arbiter.io.ifu <> cpu.io.axi_if
  Arbiter.io.lsu <> cpu.io.axi_mem
  cpu.io.interrupt := io.interrupt

  // Connect Master
  io.master <> Arbiter.io.slave
  
  // Tie off Slave
  io.slave.awready := false.B
  io.slave.wready := false.B
  io.slave.bvalid := false.B
  io.slave.bresp := 0.U
  io.slave.bid := 0.U
  io.slave.arready := false.B
  io.slave.rvalid := false.B
  io.slave.rdata := 0.U
  io.slave.rresp := 0.U
  io.slave.rlast := false.B
  io.slave.rid := 0.U
}
