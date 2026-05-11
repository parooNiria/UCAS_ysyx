package npc

import chisel3._
import chisel3.util._

class AxiLiteXbar extends Module {
  val io = IO(new Bundle {
    val in = Flipped(new AXI4Lite)
    val sram = new AXI4Lite
    val uart = new AXI4Lite
    val clint = new AXI4Lite
  })

  // Address range checks
  def isUart(addr: UInt): Bool = addr >= "hA00003F8".U && addr < "hA0000400".U
  def isClint(addr: UInt): Bool = addr >= "hA0000048".U && addr < "hA0000050".U
  def isSram(addr: UInt): Bool = addr >= "h80000000".U && addr < "h81000000".U
  def isDevice(addr: UInt): Bool = isUart(addr) || isClint(addr)

  // Read targets
  val readSram = isSram(io.in.araddr)
  val readUart = isUart(io.in.araddr)
  val readClint = isClint(io.in.araddr)

  // Write targets
  val writeSram = isSram(io.in.awaddr)
  val writeUart = isUart(io.in.awaddr)
  val writeClint = isClint(io.in.awaddr)

  // Read channel routing with timing optimization
  val r_target = RegInit(0.U(2.W))
  val r_busy = RegInit(false.B)
  val r_match = Wire(UInt(2.W))
  val r_decerr_pending = RegInit(false.B)

  val cur_read_target = Mux(readSram, 1.U, Mux(readUart, 2.U, Mux(readClint, 3.U, 0.U)))

  when(io.in.arvalid && io.in.arready && !r_busy && !r_decerr_pending) {
    when(cur_read_target === 0.U) {
      r_decerr_pending := true.B
    } .otherwise {
      r_busy := true.B
      r_target := cur_read_target
    }
  } .elsewhen(io.in.rvalid && io.in.rready) {
    r_busy := false.B
    r_decerr_pending := false.B
  }

  r_match := Mux(!r_busy, cur_read_target, r_target)

  io.sram.arvalid := io.in.arvalid && (r_match === 1.U) && !r_busy && !r_decerr_pending
  io.uart.arvalid := io.in.arvalid && (r_match === 2.U) && !r_busy && !r_decerr_pending
  io.clint.arvalid := io.in.arvalid && (r_match === 3.U) && !r_busy && !r_decerr_pending

  io.sram.araddr := io.in.araddr
  io.uart.araddr := io.in.araddr
  io.clint.araddr := io.in.araddr

  io.in.arready := Mux(r_decerr_pending, false.B, MuxLookup(r_match, true.B)(Seq(
    1.U -> io.sram.arready,
    2.U -> io.uart.arready,
    3.U -> io.clint.arready
  )))

  io.sram.rready := io.in.rready && (r_match === 1.U) && r_busy
  io.uart.rready := io.in.rready && (r_match === 2.U) && r_busy
  io.clint.rready := io.in.rready && (r_match === 3.U) && r_busy

  io.in.rvalid := Mux(r_decerr_pending, true.B, MuxLookup(r_match, false.B)(Seq(
    1.U -> io.sram.rvalid,
    2.U -> io.uart.rvalid,
    3.U -> io.clint.rvalid
  )))

  io.in.rdata := Mux(r_decerr_pending, 0.U, MuxLookup(r_match, 0.U)(Seq(
    1.U -> io.sram.rdata,
    2.U -> io.uart.rdata,
    3.U -> io.clint.rdata
  )))

  io.in.rresp := Mux(r_decerr_pending, 3.U, MuxLookup(r_match, 0.U)(Seq(
    1.U -> io.sram.rresp,
    2.U -> io.uart.rresp,
    3.U -> io.clint.rresp
  )))

  // Write channel routing with timing optimization
  val w_target = RegInit(0.U(2.W))
  val w_busy = RegInit(false.B)
  val w_match = Wire(UInt(2.W))
  val w_decerr_pending = RegInit(false.B)

  val cur_write_target = Mux(writeSram, 1.U, Mux(writeUart, 2.U, Mux(writeClint, 3.U, 0.U)))

  when(io.in.awvalid && io.in.awready && !w_busy && !w_decerr_pending) {
    when(cur_write_target === 0.U) {
      w_decerr_pending := true.B
    } .otherwise {
      w_busy := true.B
      w_target := cur_write_target
    }
  } .elsewhen(io.in.bvalid && io.in.bready) {
    w_busy := false.B
    w_decerr_pending := false.B
  }

  w_match := Mux(!w_busy, cur_write_target, w_target)

  io.sram.awvalid := io.in.awvalid && (w_match === 1.U) && !w_busy && !w_decerr_pending
  io.uart.awvalid := io.in.awvalid && (w_match === 2.U) && !w_busy && !w_decerr_pending
  io.clint.awvalid := io.in.awvalid && (w_match === 3.U) && !w_busy && !w_decerr_pending

  io.sram.wvalid := io.in.wvalid && (w_match === 1.U) && !w_decerr_pending
  io.uart.wvalid := io.in.wvalid && (w_match === 2.U) && !w_decerr_pending
  io.clint.wvalid := io.in.wvalid && (w_match === 3.U) && !w_decerr_pending

  io.sram.awaddr := io.in.awaddr
  io.uart.awaddr := io.in.awaddr
  io.clint.awaddr := io.in.awaddr

  io.sram.wdata := io.in.wdata
  io.uart.wdata := io.in.wdata
  io.clint.wdata := io.in.wdata

  io.sram.wstrb := io.in.wstrb
  io.uart.wstrb := io.in.wstrb
  io.clint.wstrb := io.in.wstrb

  io.in.awready := Mux(w_decerr_pending, false.B, MuxLookup(w_match, true.B)(Seq(
    1.U -> io.sram.awready,
    2.U -> io.uart.awready,
    3.U -> io.clint.awready
  )))

  io.in.wready := Mux(w_decerr_pending, false.B, MuxLookup(w_match, true.B)(Seq(
    1.U -> io.sram.wready,
    2.U -> io.uart.wready,
    3.U -> io.clint.wready
  )))

  io.sram.bready := io.in.bready && (w_match === 1.U) && w_busy
  io.uart.bready := io.in.bready && (w_match === 2.U) && w_busy
  io.clint.bready := io.in.bready && (w_match === 3.U) && w_busy

  io.in.bvalid := Mux(w_decerr_pending, true.B, MuxLookup(w_match, false.B)(Seq(
    1.U -> io.sram.bvalid,
    2.U -> io.uart.bvalid,
    3.U -> io.clint.bvalid
  )))

  io.in.bresp := Mux(w_decerr_pending, 3.U, MuxLookup(w_match, 0.U)(Seq(
    1.U -> io.sram.bresp,
    2.U -> io.uart.bresp,
    3.U -> io.clint.bresp
  )))
}
