package npc

import chisel3._
import chisel3.util._

class IDU extends Module {
    val io = IO(new Bundle {
        val in = Flipped(Decoupled(new MessageIF))
        val out = Decoupled(new MessageID)
        val rf_read = new rf_read
    })

    val valid = RegInit(false.B)
    val handshake_fd = Wire(Bool())
    val handshake_de = Wire(Bool())
    handshake_de := io.in.valid && io.in.ready
    handshake_fd := io.out.valid && io.out.ready
    when(handshake_de) {
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

    io.rf_read.raddr1 := rs1
    io.rf_read.raddr2 := rs2

    io.out.bits := 0.U.asTypeOf(new MessageID)


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
    val rs1_eq_rs2 = io.rf_read.rdata1 === io.rf_read.rdata2
    val rs1_lt_rs2 = io.rf_read.rdata1.asSInt < io.rf_read.rdata2.asSInt
    val rs1_ltu_rs2 = io.rf_read.rdata1 < io.rf_read.rdata2
    br_taken := Mux(inst_beq, rs1_eq_rs2,
                Mux(inst_bne, !rs1_eq_rs2,
                Mux(inst_blt, rs1_lt_rs2,
                Mux(inst_bge, !rs1_lt_rs2,
                Mux(inst_bltu, rs1_ltu_rs2,
                Mux(inst_bgeu, !rs1_ltu_rs2, false.B))))))||
                is_j || (inst_jalr) 
                      
   
    br_target := Mux(is_b, pc_reg + br_offs,
                Mux(inst_jalr, (io.rf_read.rdata1 + jal_offs) & ~1.U(32.W),
                Mux(is_j, pc_reg + jal_offs, 0.U)))
    io.out.bits.next_branch_pc := Mux(br_taken, br_target, seq_pc)
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
        .elsewhen(func3 === 7.U) { io.out.bits.alu_op := 1.U << 4 }
    }
    .elsewhen(opcode === "b0010011".U) {
        when(func3 === 0.U) { io.out.bits.alu_op := 1.U << 0 }
        .elsewhen(func3 === 1.U) { io.out.bits.alu_op := 1.U << 7 }
        .elsewhen(func3 === 2.U) { io.out.bits.alu_op := 1.U << 2 }
        .elsewhen(func3 === 3.U) { io.out.bits.alu_op := 1.U << 3 }
        .elsewhen(func3 === 4.U) { io.out.bits.alu_op := 1.U << 6 }
        .elsewhen(func3 === 5.U) { io.out.bits.alu_op := Mux(func7(5), 1.U << 9, 1.U << 8) }
        .elsewhen(func3 === 6.U) { io.out.bits.alu_op := 1.U << 5 }
        .elsewhen(func3 === 7.U) { io.out.bits.alu_op := 1.U << 4 }
    }
    .otherwise {
        io.out.bits.alu_op := 1.U << 0 // 默认 Add
    }
    val src1_is_pc = is_u | is_j | inst_jalr //lui不用src1，所以可以is_u
    val src2_is_imm = is_i | is_s | is_u | is_j
    io.out.bits.alu_src1 := Mux(src1_is_pc, pc_reg, io.rf_read.rdata1)
    io.out.bits.alu_src2 := Mux(src2_is_imm, imm, io.rf_read.rdata2)
    //可能是csr中来自imm的值，csr来自rs1的值用于存入csr
    //store中来自rs2的值
    val imm_csr = Cat(Fill(27, 0.U), rs1)
    val csr_write_data = Mux(!func3(2), io.rf_read.rdata1, imm_csr) // csrrw和csrrwi
    io.out.bits.write_data := Mux(is_csr, csr_write_data, io.rf_read.rdata2)
    val gr_we = !is_s && !is_b && !is_sys
    io.out.bits.reg_csr_mem_en_dest := Cat(is_csr, is_load, gr_we, rd)
    io.out.bits.mem_en_LS_Type := Cat(is_load|is_store, is_load, func3)
    io.out.bits.sys_message := Cat(inst_ecall, inst_ebreak, inst_mret)
    io.out.valid := valid
    io.out.bits.inst := inst_reg
    io.out.bits.pc := pc_reg
    io.in.ready := 1.B
}