package npc

import chisel3._
import chisel3.util._

class IFUPerfStall extends Bundle {
    val stall_ar = Bool()   // waiting for AXI AR ready
    val stall_r  = Bool()   // waiting for AXI read data
    val stall_bp = Bool()   // backpressure from downstream
}

class if_sram extends Bundle { 
    val addr = Output(UInt(32.W))
    val req_valid = Output(Bool())
    val addr_ok = Input(Bool())
    val rdata = Input(UInt(32.W))
    val data_ok = Input(Bool())
}

class IFU extends Module {
  val io = IO(new Bundle {
    val out = Decoupled(new MessageIF)
    val if_sram = new if_sram
    val commit_info = Flipped(new CommitUpdate)
    val perf_stall = Output(new IFUPerfStall)
  })
    
    // 移位器: reset.asBool作串行输入, 复位时不强制清零, 让1正常移入
    val start_state = RegInit(3.U(2.W))
    start_state := Cat(start_state(0), 0.U(1.W))
    val start = start_state(1) && ~start_state(0)
    val valid = RegInit(false.B)
    when(start) {
        valid := true.B
    } .elsewhen(io.out.valid && io.out.ready) {
        valid := false.B
    } .elsewhen(io.commit_info.commit_valid) {
        valid := true.B
    }

    val pc = RegInit("h30000000".U(32.W)) 
    when(io.commit_info.commit_valid) {
        pc := io.commit_info.next_pc
    }
    val inst_reg = Reg(UInt(32.W))
    val inst_get = RegInit(false.B)
    val req_send = RegInit(false.B)
    when(io.out.ready && io.out.valid){
        req_send := false.B
    } .elsewhen(io.if_sram.addr_ok) {
        req_send := true.B
    }
    when(io.out.ready && io.out.valid){
        inst_get := false.B
    }.elsewhen(io.if_sram.data_ok){
        inst_get := true.B
    }
    when(io.if_sram.data_ok){
        inst_reg := io.if_sram.rdata
    }
    io.if_sram.addr := pc
    io.if_sram.req_valid := valid && !req_send
    
    io.out.bits.inst := Mux(inst_get, inst_reg, io.if_sram.rdata)
    io.out.bits.pc := pc
    io.out.valid := valid && (inst_get || io.if_sram.data_ok)

    // ── Performance counter: IFU stall reasons ──
    io.perf_stall.stall_ar := valid && !req_send
    io.perf_stall.stall_r  := valid && !inst_get && req_send
    io.perf_stall.stall_bp := ~valid
}