package npc

import chisel3._
import chisel3.util._

// Performance counter events from IDU
class IDUPerfEvents extends Bundle {
    val compute = Bool()   // ALU / R-type / I-type / U-type / shift
    val branch  = Bool()   // B-type
    val jump    = Bool()   // JAL / JALR
    val load    = Bool()   // load
    val store   = Bool()   // store
    val csr     = Bool()   // CSR
    val system  = Bool()   // ECALL / EBREAK / MRET
}

class IDU extends Module {
    val io = IO(new Bundle {
        val in = Flipped(Decoupled(new MessageIF))
        val out = Decoupled(new MessageID)
        val rf_read = new rf_read
        val flush_valid_in = Input(Bool())
        val flush_re_pc = Output(UInt(32.W))
        val flush_valid_out = Output(Bool())
        val reg_forward_exe = Flipped(new reg_forward)
        val reg_forward_mem = Flipped(new reg_forward)
        val reg_forward_wb = Flipped(new reg_forward)
        // Performance counter event outputs (pulsed on out.fire)
        val perf_events = Output(new IDUPerfEvents)
    })

    val valid = RegInit(false.B)
    val handshake_fd = Wire(Bool())
    val handshake_de = Wire(Bool())
    handshake_de := io.in.valid && io.in.ready
    handshake_fd := io.out.valid && io.out.ready
    
    when(io.flush_valid_in) {
        valid := false.B
    }.elsewhen(handshake_de) {
        valid := true.B
    } .elsewhen(handshake_fd) {
        valid := false.B
    } 

    val inst_reg = Reg(UInt(32.W))
    val pc_reg = Reg(UInt(32.W))
    when(handshake_de) {
        inst_reg := io.in.bits.inst
        pc_reg := io.in.bits.pc
    }

    val opcode = Wire(UInt(7.W))
    val func3  = Wire(UInt(3.W))
    val func7  = Wire(UInt(7.W))
    val rd     = Wire(UInt(5.W))
    val rs1    = Wire(UInt(5.W))
    val rs2    = Wire(UInt(5.W))
    opcode := inst_reg(6, 0)       
    func3  := inst_reg(14, 12)
    func7  := inst_reg(31, 25)
    rd     := inst_reg(11, 7)
    rs1    := inst_reg(19, 15)
    rs2    := inst_reg(24, 20)

    import chisel3.util.experimental.decode._
    //译码部分
    val dec = decoder(inst_reg, TruthTable(
        Map(
        //U型指令
        BitPat("b????????????????????_?????_0110111") -> BitPat("b1_1000_00000"), // lui
        BitPat("b????????????????????_?????_0010111") -> BitPat("b1_1000_00000"), // auipc
        //J型指令
        BitPat("b????????????????????_?????_1101111") -> BitPat("b1_0001_00000"), // jal
        //jalr是I型指令
        BitPat("b????????????_?????_000_?????_1100111") -> BitPat("b1_0010_00000"), // jalr
        //B型指令
        BitPat("b???????_?????_?????_000_?????_1100011") -> BitPat("b1_0000_10000"), // beq
        BitPat("b???????_?????_?????_001_?????_1100011") -> BitPat("b1_0000_10000"), // bne
        BitPat("b???????_?????_?????_100_?????_1100011") -> BitPat("b1_0000_10000"), // blt
        BitPat("b???????_?????_?????_101_?????_1100011") -> BitPat("b1_0000_10000"), // bge
        BitPat("b???????_?????_?????_110_?????_1100011") -> BitPat("b1_0000_10000"), // bltu
        BitPat("b???????_?????_?????_111_?????_1100011") -> BitPat("b1_0000_10000"), // bgeu
        
        BitPat("b????????????_?????_000_?????_0000011") -> BitPat("b1_0010_00010"), // lb
        BitPat("b????????????_?????_001_?????_0000011") -> BitPat("b1_0010_00010"), // lh
        BitPat("b????????????_?????_010_?????_0000011") -> BitPat("b1_0010_00010"), // lw
        BitPat("b????????????_?????_100_?????_0000011") -> BitPat("b1_0010_00010"), // lbu
        BitPat("b????????????_?????_101_?????_0000011") -> BitPat("b1_0010_00010"), // lhu
        
        BitPat("b???????_?????_?????_000_?????_0100011") -> BitPat("b1_0100_00001"), // sb
        BitPat("b???????_?????_?????_001_?????_0100011") -> BitPat("b1_0100_00001"), // sh
        BitPat("b???????_?????_?????_010_?????_0100011") -> BitPat("b1_0100_00001"), // sw
        
        BitPat("b????????????_?????_000_?????_0010011") -> BitPat("b1_0010_00000"), // addi
        BitPat("b????????????_?????_010_?????_0010011") -> BitPat("b1_0010_00000"), // slti
        BitPat("b????????????_?????_011_?????_0010011") -> BitPat("b1_0010_00000"), // sltiu
        BitPat("b????????????_?????_100_?????_0010011") -> BitPat("b1_0010_00000"), // xori
        BitPat("b????????????_?????_110_?????_0010011") -> BitPat("b1_0010_00000"), // ori
        BitPat("b????????????_?????_111_?????_0010011") -> BitPat("b1_0010_00000"), // andi
        BitPat("b0000000_?????_?????_001_?????_0010011") -> BitPat("b1_0010_00000"), // slli
        BitPat("b0000000_?????_?????_101_?????_0010011") -> BitPat("b1_0010_00000"), // srli
        BitPat("b0100000_?????_?????_101_?????_0010011") -> BitPat("b1_0010_00000"), // srai
        
        //R型指令
        BitPat("b0000000_?????_?????_???_?????_0110011") -> BitPat("b1_0000_00000"), // add/sll/..
        BitPat("b0100000_?????_?????_000_?????_0110011") -> BitPat("b1_0000_00000"), // sub
        BitPat("b0100000_?????_?????_101_?????_0110011") -> BitPat("b1_0000_00000"), // sra
        
        BitPat("b000000000001_00000_000_00000_1110011") -> BitPat("b1_0000_01000"), // ebreak
        BitPat("b000000000000_00000_000_00000_1110011") -> BitPat("b1_0000_01000"), // ecall
        BitPat("b0011000_00010_00000_000_00000_1110011") -> BitPat("b1_0000_01000"), // mret
        BitPat("b000000000000_00000_001_00000_0001111") -> BitPat("b1_0000_01000"), // fence.i
        //CSR指令
        BitPat("b????????????_?????_001_?????_1110011") -> BitPat("b1_0000_00100"), // csrrw
        BitPat("b????????????_?????_010_?????_1110011") -> BitPat("b1_0000_00100"), // csrrs
        BitPat("b????????????_?????_011_?????_1110011") -> BitPat("b1_0000_00100"), // csrrc
        BitPat("b????????????_?????_101_?????_1110011") -> BitPat("b1_0000_00100"), // csrrwi
        BitPat("b????????????_?????_110_?????_1110011") -> BitPat("b1_0000_00100"), // csrrsi
        BitPat("b????????????_?????_111_?????_1110011") -> BitPat("b1_0000_00100")  // csrrci
        ),
        BitPat("b0_????_?????") // {is_val[9], is_u[8], is_s[7], is_i[6], is_j[5], is_b[4], is_sys[3], is_csr[2], is_load[1], is_store[0]}
    ))

    val is_val  = dec(9)
    val is_u    = dec(8)
    val is_s    = dec(7)
    val is_i    = dec(6)
    val is_j    = dec(5)
    val is_b    = dec(4)
    val is_sys  = dec(3)
    val is_csr  = dec(2)
    val is_load = dec(1)
    val is_store= dec(0)
    
    val inst_inv = !is_val
    
    def inst_eq(pat: String) = inst_reg === BitPat("b" + pat)
    
    val inst_jalr = inst_eq("????????????_?????_000_?????_1100111")
    val inst_beq  = inst_eq("???????_?????_?????_000_?????_1100011")
    val inst_bne  = inst_eq("???????_?????_?????_001_?????_1100011")
    val inst_blt  = inst_eq("???????_?????_?????_100_?????_1100011")
    val inst_bge  = inst_eq("???????_?????_?????_101_?????_1100011")
    val inst_bltu = inst_eq("???????_?????_?????_110_?????_1100011")
    val inst_bgeu = inst_eq("???????_?????_?????_111_?????_1100011")
    val inst_mret = inst_eq("0011000_00010_00000_000_00000_1110011")
    val inst_ecall = inst_eq("000000000000_00000_000_00000_1110011")
    val inst_ebreak = inst_eq("000000000001_00000_000_00000_1110011")
    val inst_fencei = inst_eq("000000000000_00000_001_00000_0001111")

    io.rf_read.raddr1 := rs1
    io.rf_read.raddr2 := rs2

    //接受到指令，得到译码，开始分析产生信号
    //首先产生是否存在寄存器冲突的信号
    val no_need_rs1 = Wire(Bool())
    val no_need_rs2 = Wire(Bool())
    //jal,lui,auipc,sys，crsi
    no_need_rs1 := is_u | is_j | is_sys | (is_csr && func3(2))
    //csr,sys,u型，I型
    no_need_rs2 := is_i | is_sys | is_u | is_csr

    val rs1_same_with_exe = Wire(Bool())
    val rs1_same_with_mem = Wire(Bool())
    val rs1_same_with_wb  = Wire(Bool())
    val rs2_same_with_exe = Wire(Bool())
    val rs2_same_with_mem = Wire(Bool())
    val rs2_same_with_wb  = Wire(Bool())
    val rs1_confict_with_exe = Wire(Bool())
    val rs1_confict_with_mem = Wire(Bool())
    val rs2_confict_with_exe = Wire(Bool())
    val rs2_confict_with_mem = Wire(Bool())
    val rs1_confict = Wire(Bool())
    val rs2_confict = Wire(Bool())

    rs1_same_with_exe := rs1 =/= 0.U && rs1 === io.reg_forward_exe.reg_dest && io.reg_forward_exe.ref_dest_en
    rs1_same_with_mem := rs1 =/= 0.U && rs1 === io.reg_forward_mem.reg_dest && io.reg_forward_mem.ref_dest_en
    rs1_same_with_wb := rs1 =/= 0.U && rs1 === io.reg_forward_wb.reg_dest && io.reg_forward_wb.ref_dest_en
    rs2_same_with_exe := rs2 =/= 0.U && rs2 === io.reg_forward_exe.reg_dest && io.reg_forward_exe.ref_dest_en
    rs2_same_with_mem := rs2 =/= 0.U && rs2 === io.reg_forward_mem.reg_dest && io.reg_forward_mem.ref_dest_en
    rs2_same_with_wb := rs2 =/= 0.U && rs2 === io.reg_forward_wb.reg_dest && io.reg_forward_wb.ref_dest_en

    rs1_confict_with_exe := rs1_same_with_exe && !io.reg_forward_exe.reg_data_en
    rs1_confict_with_mem := rs1_same_with_mem && !io.reg_forward_mem.reg_data_en
    rs2_confict_with_exe := rs2_same_with_exe && !io.reg_forward_exe.reg_data_en
    rs2_confict_with_mem := rs2_same_with_mem && !io.reg_forward_mem.reg_data_en


    rs1_confict := rs1_confict_with_exe || rs1_confict_with_mem
    rs2_confict := rs2_confict_with_exe || rs2_confict_with_mem 
    val data_conflict = (rs1_confict && !no_need_rs1) || (rs2_confict && !no_need_rs2)

    val rdata_rs1 = Wire(UInt(32.W))
    val rdata_rs2 = Wire(UInt(32.W))

    rdata_rs1 := Mux(rs1_same_with_exe && io.reg_forward_exe.reg_data_en, io.reg_forward_exe.reg_forward_data,
                Mux(rs1_same_with_mem && io.reg_forward_mem.reg_data_en, io.reg_forward_mem.reg_forward_data,
                Mux(rs1_same_with_wb && io.reg_forward_wb.reg_data_en, io.reg_forward_wb.reg_forward_data,
                io.rf_read.rdata1)))
    rdata_rs2 := Mux(rs2_same_with_exe && io.reg_forward_exe.reg_data_en, io.reg_forward_exe.reg_forward_data,
                Mux(rs2_same_with_mem && io.reg_forward_mem.reg_data_en, io.reg_forward_mem.reg_forward_data,
                Mux(rs2_same_with_wb && io.reg_forward_wb.reg_data_en, io.reg_forward_wb.reg_forward_data,
                io.rf_read.rdata2)))

    //其次开始分析控制流相关的部分
    val imm          = Wire(UInt(32.W))
    val br_offs      = Wire(UInt(32.W))
    val jal_offs     = Wire(UInt(32.W))
    val br_taken     = Wire(Bool())
    val br_target    = Wire(UInt(32.W))
    
    val src2_is_4 = is_j | inst_jalr
    imm := Mux(src2_is_4, 4.U(32.W),
         Mux(is_i, Cat(Fill(20, inst_reg(31)), inst_reg(31, 20)),
         Mux(is_s, Cat(Fill(20, inst_reg(31)), inst_reg(31, 25), inst_reg(11, 7)),
         Mux(is_u, Cat(inst_reg(31, 12), 0.U(12.W)), 0.U))))

    br_offs := Cat(Fill(20, inst_reg(31)), inst_reg(7), inst_reg(30, 25), inst_reg(11, 8), 0.U(1.W))
    jal_offs := Mux(is_j, Cat(Fill(12, inst_reg(31)), inst_reg(19, 12), inst_reg(20), inst_reg(30, 21), 0.U(1.W)),
                        Cat(Fill(20, inst_reg(31)), inst_reg(31, 20)))
    
    val seq_pc = pc_reg + 4.U
    val rs1_eq_rs2 = rdata_rs1 === rdata_rs2
    val rs1_lt_rs2 = rdata_rs1.asSInt < rdata_rs2.asSInt
    val rs1_ltu_rs2 = rdata_rs1 < rdata_rs2
    br_taken := Mux(inst_beq, rs1_eq_rs2,
                Mux(inst_bne, !rs1_eq_rs2,
                Mux(inst_blt, rs1_lt_rs2,
                Mux(inst_bge, !rs1_lt_rs2,
                Mux(inst_bltu, rs1_ltu_rs2,
                Mux(inst_bgeu, !rs1_ltu_rs2, false.B))))))||
                is_j || (inst_jalr)
                      
    br_target := Mux(is_b, pc_reg + br_offs,
                Mux(inst_jalr, (rdata_rs1 + jal_offs) & ~1.U(32.W),
                Mux(is_j, pc_reg + jal_offs, 0.U)))

    //此次开始分析执行流相关的部分
    when(opcode === "b0110111".U) { 
        io.out.bits.alu_op := 1.U << 10 
    } // LUI
    .elsewhen(opcode === "b0110011".U) {
        when(func3 === 0.U) { io.out.bits.alu_op := Mux(func7(5), 1.U << 1, 1.U << 0) }
        .elsewhen(func3 === 1.U) { io.out.bits.alu_op := 1.U << 7 }
        .elsewhen(func3 === 2.U) { io.out.bits.alu_op := 1.U << 2 }
        .elsewhen(func3 === 3.U) { io.out.bits.alu_op := 1.U << 3 }
        .elsewhen(func3 === 4.U) { io.out.bits.alu_op := 1.U << 6 }
        .elsewhen(func3 === 5.U) { io.out.bits.alu_op := Mux(func7(5), 1.U << 9, 1.U << 8) }
        .elsewhen(func3 === 6.U) { io.out.bits.alu_op := 1.U << 5 }
        .otherwise { io.out.bits.alu_op := 1.U << 4 }
    }
    .elsewhen(opcode === "b0010011".U) {
        when(func3 === 0.U) { io.out.bits.alu_op := 1.U << 0 }
        .elsewhen(func3 === 1.U) { io.out.bits.alu_op := 1.U << 7 }
        .elsewhen(func3 === 2.U) { io.out.bits.alu_op := 1.U << 2 }
        .elsewhen(func3 === 3.U) { io.out.bits.alu_op := 1.U << 3 }
        .elsewhen(func3 === 4.U) { io.out.bits.alu_op := 1.U << 6 }
        .elsewhen(func3 === 5.U) { io.out.bits.alu_op := Mux(func7(5), 1.U << 9, 1.U << 8) }
        .elsewhen(func3 === 6.U) { io.out.bits.alu_op := 1.U << 5 }
        .otherwise { io.out.bits.alu_op := 1.U << 4 }
    }
    .otherwise {
        io.out.bits.alu_op := 1.U << 0 // 默认 Add
    }
    val src1_is_pc = is_u | is_j | inst_jalr //lui不用src1，所以可以is_u
    val src2_is_imm = is_i | is_s | is_u | is_j
    io.out.bits.alu_src1 := Mux(src1_is_pc, pc_reg, rdata_rs1)
    io.out.bits.alu_src2 := Mux(src2_is_imm, imm, rdata_rs2)
    //可能是csr中来自imm的值，csr来自rs1的值用于存入csr
    //store中来自rs2的值
    val imm_csr = Cat(Fill(27, 0.U), rs1)
    val csr_write_data = Mux(!func3(2), rdata_rs1, imm_csr) // csrrw和csrrwi
    io.out.bits.write_data := Mux(is_csr, csr_write_data, rdata_rs2)
    val gr_we = !is_s && !is_b && !is_sys
    io.out.bits.reg_csr_mem_en_dest := Cat(is_csr, is_load, gr_we, rd)
    io.out.bits.mem_en_LS_Type := Cat(is_load|is_store, is_load, func3)

    io.out.bits.inst := inst_reg
    io.out.bits.pc := pc_reg
    io.out.bits.next_pc := Mux(br_taken, br_target, pc_reg + 4.U)
    io.in.ready := !valid || (io.out.valid && io.out.ready)
    io.out.valid := !data_conflict && valid && !io.flush_valid_in

    val exp_status = inst_inv || inst_ebreak | inst_ecall | inst_fencei | inst_mret
    io.flush_valid_out := valid && (br_taken || exp_status) && !data_conflict
    io.flush_re_pc := br_target
    io.out.bits.ExpMessage.ebreak := inst_ebreak
    io.out.bits.ExpMessage.ecall := inst_ecall
    io.out.bits.ExpMessage.mret := inst_mret
    io.out.bits.ExpMessage.fencei := inst_fencei
    io.out.bits.ExpMessage.inv_inst := inst_inv


    // ── Performance counter events: pulse on out.fire ──
    val idu_fire = io.out.valid && io.out.ready
    io.perf_events.compute := idu_fire && is_val && !is_load && !is_store &&
                               !is_b && !is_j && !is_sys && !is_csr
    io.perf_events.branch  := idu_fire && is_b
    io.perf_events.jump    := idu_fire && (is_j || inst_jalr)
    io.perf_events.load    := idu_fire && is_load
    io.perf_events.store   := idu_fire && is_store
    io.perf_events.csr     := idu_fire && is_csr
    io.perf_events.system  := idu_fire && is_sys
}