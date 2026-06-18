package npc

import chisel3._
import chisel3.util._


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
    val flush_valid = Input(Bool())
    val flush_pc = Input(UInt(32.W))
    val perf_read_req = Output(Bool())
  })
    
    // 移位器: reset.asBool作串行输入, 复位时不强制清零, 让1正常移入
    //  依旧采取ca中，对于if级分为pre_if和if两个部分，pre_if负责发请求，if负责接受请求
    val start_state = RegInit(3.U(2.W))
    start_state := Cat(start_state(0), 0.U(1.W))
    val start = start_state(1) && ~start_state(0)
    val valid = RegInit(false.B)
    when(start) {
        valid := true.B
    }

    val pre_pc = RegInit("h30000000".U(32.W)) 
    when(io.flush_valid) {
        pre_pc := io.flush_pc
    } .elsewhen(io.if_sram.req_valid && io.if_sram.addr_ok) {
        pre_pc := pre_pc + 4.U
    }
    io.if_sram.addr :=  pre_pc

    val if_valid = RegInit(false.B)
    val if_req_valid = RegInit(false.B)

    //什么时候能发请求呢
    //首先需要下一级没有请求，或者请求即将完成
    //其次，需要下一级为空，或者即将为空
    io.if_sram.req_valid := (!if_req_valid || io.if_sram.data_ok) &&
                           (!if_valid || (io.out.valid && io.out.ready)) &&
                           (!io.flush_valid)
    when(io.if_sram.req_valid && io.if_sram.addr_ok) {
        if_req_valid := true.B
    } .elsewhen(io.if_sram.data_ok){
        if_req_valid := false.B
    }

    when(io.flush_valid){
        if_valid := false.B
    } .elsewhen(io.if_sram.req_valid && io.if_sram.addr_ok){
        if_valid := true.B
    } .elsewhen(io.out.valid && io.out.ready){
        if_valid := false.B
    }

    val pc_reg = Reg(UInt(32.W))
    when(io.if_sram.req_valid && io.if_sram.addr_ok) {
        pc_reg := io.if_sram.addr
    }
    val inst_reg = Reg(UInt(32.W))
    
    when(io.if_sram.data_ok){
        inst_reg := io.if_sram.rdata
    }
    
    io.out.bits.inst := Mux(io.if_sram.data_ok, io.if_sram.rdata,inst_reg)
    io.out.bits.pc := pc_reg
    //什么时候能够发出给下一级的有效呢
    //首先需要本级有效
    //其次要有指令
    io.out.valid := (if_valid && !io.flush_valid) &&
                    (io.if_sram.data_ok || !if_req_valid)

    // ── Performance counter: IFU stall reasons ──
    io.perf_read_req := io.if_sram.req_valid && io.if_sram.addr_ok
}