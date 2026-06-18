package npc

import chisel3._
import chisel3.util._
class MEMU extends Module {
    val io = IO(new Bundle {
        val in = Flipped(Decoupled(new MessageEXE))
        val out = Decoupled(new MessageMEM)
        val reg_forward = new reg_forward
        val exp_status = Output(Bool())
        
        val rdata  = Input(UInt(32.W))
        val rresp  = Input(UInt(2.W))
        val rvalid = Input(Bool())
        val rready = Output(Bool())
        val rlast  = Input(Bool())
        val rid    = Input(UInt(4.W))
        
        val bresp  = Input(UInt(2.W))
        val bvalid = Input(Bool())
        val bready = Output(Bool())
        val bid    = Input(UInt(4.W))

        // Performance counter events
        val perf_load  = Output(Bool())  // load data received
        val perf_store = Output(Bool())  // store response received
    })

    val valid = RegInit(false.B)
    val handshake_em = io.in.valid && io.in.ready
    val handshake_mw = io.out.valid && io.out.ready
    when(handshake_em) {
        valid := true.B
    } .elsewhen(!handshake_em && handshake_mw) {
        valid := false.B
    }

    val inst_reg = Reg(UInt(32.W))
    val pc_reg = Reg(UInt(32.W))
    val next_pc_reg = Reg(UInt(32.W))
    val alu_result_reg = Reg(UInt(32.W))
    val write_data_csr_reg = Reg(UInt(32.W))
    val mem_en_LS_Type_reg = Reg(UInt(5.W))
    val reg_csr_mem_en_dest_reg = Reg(UInt(8.W))
    val ExpMessage_reg = Reg(new ExpMessage)
    val device_access_reg = Reg(Bool())
    when(handshake_em) {
        inst_reg := io.in.bits.inst
        pc_reg := io.in.bits.pc
        next_pc_reg := io.in.bits.next_pc
        alu_result_reg := io.in.bits.alu_result
        write_data_csr_reg := io.in.bits.write_data_csr
        mem_en_LS_Type_reg := io.in.bits.mem_en_LS_Type
        reg_csr_mem_en_dest_reg := io.in.bits.reg_csr_mem_en_dest
        device_access_reg := io.in.bits.device_access
        ExpMessage_reg := io.in.bits.ExpMessage
    }

    val ExpStatus_in = ExpMessage_reg.ebreak || ExpMessage_reg.ecall || ExpMessage_reg.inv_inst || ExpMessage_reg.mret || ExpMessage_reg.fencei
    io.exp_status := ExpStatus_in && valid

    val mem_en = mem_en_LS_Type_reg(4)
    val is_load = mem_en_LS_Type_reg(3) && mem_en
    val is_store = !mem_en_LS_Type_reg(3) && mem_en
    val func3 = mem_en_LS_Type_reg(2, 0)
    val is_lb = is_load && func3 === "b000".U
    val is_lh = is_load && func3 === "b001".U
    val is_lw = is_load && func3 === "b010".U
    val is_lbu = is_load && func3 === "b100".U
    val is_lhu = is_load && func3 === "b101".U
    val rdata_recieve = io.rvalid && io.rid === 1.U && io.rlast
    val bresp_recieve = io.bvalid && io.bid === 1.U
    val load_already_saved = RegInit(false.B)
    val load_data = Reg(UInt(32.W))
    when (is_load && rdata_recieve) {
        load_data := io.rdata
    }
    when (handshake_mw) {
        load_already_saved := false.B
    } .elsewhen(is_load && rdata_recieve) {
        load_already_saved := true.B
    } 

    val store_already_responded = RegInit(false.B)
    when (handshake_mw) {
        store_already_responded := false.B
    } .elsewhen(is_store && bresp_recieve && io.bresp === 0.U) {
        store_already_responded := true.B
    }

    io.rready := is_load && !load_already_saved
    io.bready := is_store && !store_already_responded
    io.out.valid := valid && ((!(mem_en && !ExpStatus_in)) || (is_load && (rdata_recieve || load_already_saved)) || (is_store && bresp_recieve && io.bresp === 0.U))
    io.out.bits.inst := inst_reg
    io.out.bits.pc := pc_reg
    io.out.bits.next_pc := next_pc_reg

    val rdata = Mux(load_already_saved, load_data, io.rdata)
    val read_data_b = Mux(alu_result_reg(1, 0) === "b00".U, rdata(7, 0),
                    Mux(alu_result_reg(1, 0) === "b01".U, rdata(15, 8),
                    Mux(alu_result_reg(1, 0) === "b10".U, rdata(23, 16),
                    Mux(alu_result_reg(1, 0) === "b11".U, rdata(31, 24), 0.U))))
    val read_data_h = Mux(alu_result_reg(1) === 0.U, rdata(15, 0), rdata(31, 16))
    val read_data = Mux(is_lb, Cat(Fill(24, read_data_b(7)),read_data_b),
                    Mux(is_lh, Cat(Fill(16, read_data_h(15)), read_data_h),
                    Mux(is_lw, rdata,
                    Mux(is_lbu, Cat(Fill(24, 0.U), read_data_b),
                    Mux(is_lhu, Cat(Fill(16, 0.U), read_data_h), 0.U)))))
    
    io.out.bits.reg_write_data := Mux(is_load, read_data, alu_result_reg)
    io.out.bits.csr_write_data := write_data_csr_reg
    io.out.bits.reg_csr_en_dest := Cat(reg_csr_mem_en_dest_reg(7), reg_csr_mem_en_dest_reg(5, 0))
    io.out.bits.ExpMessage := ExpMessage_reg
    io.in.ready := !valid || (io.out.valid && io.out.ready)
    io.out.bits.device_access := device_access_reg
    io.reg_forward.reg_forward_data := io.out.bits.reg_write_data
    io.reg_forward.reg_data_en := valid && (!is_load  || (load_already_saved || rdata_recieve)) && !reg_csr_mem_en_dest_reg(7)
    io.reg_forward.reg_dest := reg_csr_mem_en_dest_reg(4, 0)
    io.reg_forward.ref_dest_en := valid &&reg_csr_mem_en_dest_reg(5)

    // ── Performance counter events ──
    io.perf_load  := is_load  && io.rvalid && io.rid === 1.U && io.rlast
    io.perf_store := is_store && io.bvalid && io.bid === 1.U && io.bresp === 0.U
}