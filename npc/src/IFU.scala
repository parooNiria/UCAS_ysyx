package npc

import chisel3._
import chisel3.util._

class IFU extends Module {
  val io = IO(new Bundle {
    val out = Decoupled(new MessageIF)
    val if_axi = new AXI4Lite
    val commit_info = Flipped(new CommitUpdate)
  })
    //写通道总是拉为0
    io.if_axi.awaddr := 0.U
    io.if_axi.awvalid := false.B
    io.if_axi.wdata := 0.U
    io.if_axi.wstrb := 0.U
    io.if_axi.wvalid := false.B
    io.if_axi.bready := false.B

    //将其划分为一个状态机
    //初始为空闲，发出pc地址后进入请求状态，等待数据返回，收到数据后进入响应状态，最后返回空闲状态
    val reset_val = reset.asBool
    val valid = RegNext(!reset_val, false.B)

    val sIdle :: sReq :: sWait :: Nil = Enum(3)
    val state = RegInit(sIdle)
    val rReq_handshake = io.if_axi.arvalid && io.if_axi.arready
    val rResp_handshake = io.if_axi.rvalid && io.if_axi.rready
    when (valid) {
        switch (state) {
            is (sIdle) {
                when (rReq_handshake) {
                    state := sReq
                }
            }
            is (sReq) {
                when (rResp_handshake) {
                    state := sWait
                }
            }
            is (sWait) {
                when (io.commit_info.commit_valid) {
                    state := sIdle
                }
            }
        }
    }

    val pc = RegInit("h80000000".U(32.W))
    when(valid && io.commit_info.commit_valid) {
        pc := io.commit_info.next_pc
    }
    val inst_reg = Reg(UInt(32.W))
    when (rResp_handshake) {
        inst_reg := io.if_axi.rdata
    }
    io.if_axi.araddr := pc
    io.if_axi.arvalid := (state === sIdle) && valid
    io.if_axi.rready := (state === sReq) && valid
    val handshake_fd = RegInit(false.B)
    when (io.out.valid && io.out.ready) {
        handshake_fd := true.B
    } .elsewhen (io.commit_info.commit_valid) {
        handshake_fd := false.B
    }
    io.out.valid := ((state === sWait)|| (state === sReq && io.if_axi.rvalid) )&& valid && !handshake_fd
    io.out.bits.pc := pc
    io.out.bits.inst := (Mux(state === sWait, inst_reg, io.if_axi.rdata))
}