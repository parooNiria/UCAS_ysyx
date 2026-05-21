package npc

import chisel3._
import chisel3.util._

class ysyx_00000000 extends Module {
  val io = IO(new Bundle {
    val interrupt = Input(Bool())
    val master = new AXI4Bundle
    val slave = Flipped(new AXI4Bundle)
  })

  val cpu = Module(new cpu)
  val Arbiter = Module(new AXI4Arbiter)


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
