package npc

import chisel3._
import chisel3.util._

class AxiLiteCLINT extends Module {
  val io = IO(new Bundle {
    val axi = Flipped(new AXI4Lite)
  })

  private val clintBase = "h02000000".U(32.W)
  private val mtimeLo = "h0200BFF8".U(32.W)
  private val mtimeHi = "h0200BFFC".U(32.W)

  val mtime = RegInit(0.U(64.W))
  mtime := mtime + 1.U

  val readPending = RegInit(false.B)
  val readData = Reg(UInt(32.W))
  val readResp = Reg(UInt(2.W))

  val writePending = RegInit(false.B)
  val writeResp = Reg(UInt(2.W))

  val arAddrWord = io.axi.araddr(31, 2) << 2

  io.axi.arready := !readPending
  io.axi.rvalid := readPending
  io.axi.rdata := readData
  io.axi.rresp := readResp

  when(!readPending && io.axi.arvalid && io.axi.arready) {
    readPending := true.B
    readResp := 0.U
    readData := Mux(arAddrWord === mtimeLo, mtime(31, 0),
      Mux(arAddrWord === mtimeHi, mtime(63, 32), 0.U))
  }
  when(readPending && io.axi.rvalid && io.axi.rready) {
    readPending := false.B
  }

  io.axi.awready := !writePending
  io.axi.wready := !writePending
  io.axi.bvalid := writePending
  io.axi.bresp := writeResp

  val writeFire = io.axi.awvalid && io.axi.awready && io.axi.wvalid && io.axi.wready
  when(!writePending && writeFire) {
    writePending := true.B
    writeResp := 3.U
  }
  when(writePending && io.axi.bvalid && io.axi.bready) {
    writePending := false.B
  }
}
