package npc

import chisel3._
import chisel3.util._

class AxiLiteUART extends Module {
  val io = IO(new Bundle {
    val axi = Flipped(new AXI4Lite)
  })

  private val uartBaseWord = "hA00003F8".U(32.W)
  private val uartLsrWord = "hA00003FC".U(32.W)
  private val uartLsrTxReady = "h20".U(8.W)

  val readPending = RegInit(false.B)
  val readData = Reg(UInt(32.W))
  val readResp = Reg(UInt(2.W))

  val writePending = RegInit(false.B)
  val writeResp = Reg(UInt(2.W))

  val arAddrWord = io.axi.araddr(31, 2) << 2
  val awAddrWord = io.axi.awaddr(31, 2) << 2

  io.axi.arready := !readPending
  io.axi.rvalid := readPending
  io.axi.rdata := readData
  io.axi.rresp := readResp

  when(!readPending && io.axi.arvalid && io.axi.arready) {
    readPending := true.B
    readResp := 0.U
    readData := Mux(arAddrWord === uartBaseWord, "hFFFFFFFF".U(32.W),
      Mux(arAddrWord === uartLsrWord, uartLsrTxReady << 8, 0.U))
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
    writeResp := 0.U
    when(awAddrWord === uartBaseWord) {
      printf("%c", io.axi.wdata(7, 0))
    }
  }
  when(writePending && io.axi.bvalid && io.axi.bready) {
    writePending := false.B
  }
}
