package npc

import chisel3._
import chisel3.util._

// AXI4仲裁器，将两个AXI4主设备端口合并到一个AXI4从设备端口
class AXI4Arbiter extends Module {
  val io = IO(new Bundle {
    val ifu = Flipped(new AXI4Bundle)
    val lsu = Flipped(new AXI4Bundle)
    val slave = new AXI4Bundle
  })

  val ar_sel_ifu = io.ifu.arvalid
  
  io.slave.arvalid := io.lsu.arvalid || io.ifu.arvalid
  io.slave.araddr  := Mux(ar_sel_ifu, io.ifu.araddr, io.lsu.araddr)
  io.slave.arlen   := Mux(ar_sel_ifu, io.ifu.arlen, io.lsu.arlen)
  io.slave.arsize  := Mux(ar_sel_ifu, io.ifu.arsize, io.lsu.arsize)
  io.slave.arburst := Mux(ar_sel_ifu, io.ifu.arburst, io.lsu.arburst)
  
  //最高位作为 Master 标识,1代表ifu,0代表lsu
  io.slave.arid    := Mux(ar_sel_ifu, Cat(1.U(1.W), io.ifu.arid(2, 0)), Cat(0.U(1.W), io.lsu.arid(2, 0)))

  // 握手信号回传
  io.ifu.arready := io.slave.arready && ar_sel_ifu
  io.lsu.arready := io.slave.arready && !ar_sel_ifu

  val r_target_ifu = io.slave.rid(3)
  io.ifu.rvalid  := io.slave.rvalid && r_target_ifu
  io.lsu.rvalid  := io.slave.rvalid && !r_target_ifu
  io.slave.rready := Mux(r_target_ifu, io.ifu.rready, io.lsu.rready)

  val rdata_shared = io.slave.rdata
  val rresp_shared = io.slave.rresp
  val rlast_shared = io.slave.rlast
  val rid_shared   = Cat(0.U(1.W), io.slave.rid(2, 0)) // 抹除最高位，还原原 ID

  io.lsu.rdata := rdata_shared
  io.lsu.rresp := rresp_shared
  io.lsu.rlast := rlast_shared
  io.lsu.rid   := rid_shared

  io.ifu.rdata := rdata_shared
  io.ifu.rresp := rresp_shared
  io.ifu.rlast := rlast_shared
  io.ifu.rid   := rid_shared

  io.slave.awvalid := io.lsu.awvalid
  io.slave.awaddr  := io.lsu.awaddr
  io.slave.awlen   := io.lsu.awlen
  io.slave.awsize  := io.lsu.awsize
  io.slave.awburst := io.lsu.awburst
  io.slave.awid    := io.lsu.awid

  io.ifu.awready := false.B
  io.lsu.awready := io.slave.awready

  io.slave.wvalid := io.lsu.wvalid
  io.slave.wdata  := io.lsu.wdata
  io.slave.wstrb  := io.lsu.wstrb
  io.slave.wlast  := io.lsu.wlast

  io.lsu.wready := io.slave.wready
  io.ifu.wready := false.B 

  io.lsu.bvalid  := io.slave.bvalid
  io.lsu.bresp   := io.slave.bresp
  io.lsu.bid     := Cat(0.U(1.W), io.slave.bid(2, 0))
  io.slave.bready := io.lsu.bready

  io.ifu.bvalid  := false.B
  io.ifu.bresp   := 0.U
  io.ifu.bid     := 0.U
}