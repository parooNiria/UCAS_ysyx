package npc

import chisel3._
import chisel3.util._

class AxiLiteArbiter extends Module {
  val io = IO(new Bundle {
    val ifu = Flipped(new AXI4Lite)
    val lsu = Flipped(new AXI4Lite)
    val mem = new AXI4Lite
  })

  // IFU never writes. Tie off its write response channels at the arbiter boundary.
  io.ifu.awready := false.B
  io.ifu.wready := false.B
  io.ifu.bresp := 0.U
  io.ifu.bvalid := false.B

  // Write channels come only from LSU.
  io.mem.awaddr := io.lsu.awaddr
  io.mem.awvalid := io.lsu.awvalid
  io.lsu.awready := io.mem.awready

  io.mem.wdata := io.lsu.wdata
  io.mem.wstrb := io.lsu.wstrb
  io.mem.wvalid := io.lsu.wvalid
  io.lsu.wready := io.mem.wready

  io.lsu.bresp := io.mem.bresp
  io.lsu.bvalid := io.mem.bvalid
  io.mem.bready := io.lsu.bready

  val readActive = RegInit(false.B)
  val readOwnerLsu = RegInit(false.B)

  val grantLsu = io.lsu.arvalid
  val grantIfu = !grantLsu && io.ifu.arvalid

  io.mem.arvalid := !readActive && (grantLsu || grantIfu)
  io.mem.araddr := Mux(grantLsu, io.lsu.araddr, io.ifu.araddr)

  io.lsu.arready := !readActive && grantLsu && io.mem.arready
  io.ifu.arready := !readActive && grantIfu && io.mem.arready

  val arFireLsu = !readActive && grantLsu && io.mem.arready
  val arFireIfu = !readActive && grantIfu && io.mem.arready
  when(arFireLsu) {
    readActive := true.B
    readOwnerLsu := true.B
  }.elsewhen(arFireIfu) {
    readActive := true.B
    readOwnerLsu := false.B
  }

  io.mem.rready := Mux(readOwnerLsu, io.lsu.rready, io.ifu.rready)

  io.lsu.rvalid := readActive && readOwnerLsu && io.mem.rvalid
  io.lsu.rdata := io.mem.rdata
  io.lsu.rresp := io.mem.rresp

  io.ifu.rvalid := readActive && !readOwnerLsu && io.mem.rvalid
  io.ifu.rdata := io.mem.rdata
  io.ifu.rresp := io.mem.rresp

  when(readActive && io.mem.rvalid && io.mem.rready) {
    readActive := false.B
  }
}
