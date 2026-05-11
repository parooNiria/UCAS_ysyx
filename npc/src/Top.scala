package npc

import chisel3._
import chisel3.util._

class top extends Module {
  val io = IO(new Bundle {
    val axi_sram = new AXI4Lite
    val pc = Output(UInt(32.W))
    val inst = Output(UInt(32.W))
    val ebreak = Output(Bool())
    val commit_valid = Output(Bool())
    val device_access = Output(Bool())
    val dbg_rf = Output(Vec(32, UInt(32.W)))
    val dbg_mstatus = Output(UInt(32.W))
    val dbg_mtvec = Output(UInt(32.W))
    val dbg_mepc = Output(UInt(32.W))
    val dbg_mcause = Output(UInt(32.W))
  })

  val core = Module(new cpu)
  val arbiter = Module(new AxiLiteArbiter)
  val xbar = Module(new AxiLiteXbar)
  val uart = Module(new AxiLiteUART)
  val clint = Module(new AxiLiteCLINT)

  arbiter.io.ifu <> core.io.axi_if
  arbiter.io.lsu <> core.io.axi_mem
  xbar.io.in <> arbiter.io.mem
  io.axi_sram <> xbar.io.sram
  xbar.io.uart <> uart.io.axi
  xbar.io.clint <> clint.io.axi

  io.pc := core.io.pc
  io.inst := core.io.inst
  io.ebreak := core.io.ebreak
  io.commit_valid := core.io.commit_valid
  io.device_access := core.io.device_access


  io.dbg_rf := core.io.dbg_rf
  io.dbg_mstatus := core.io.dbg_mstatus
  io.dbg_mtvec := core.io.dbg_mtvec
  io.dbg_mepc := core.io.dbg_mepc
  io.dbg_mcause := core.io.dbg_mcause
}
