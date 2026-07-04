package npc

import chisel3._
import chisel3.util._
// ======================================
// ALU
// ======================================
class ALU extends Module {
  val io = IO(new Bundle {
    val alu_op     = Input(UInt(11.W))
    val alu_src1   = Input(UInt(32.W))
    val alu_src2   = Input(UInt(32.W))
    val alu_result = Output(UInt(32.W))
  })

  val op_add  = io.alu_op(0)
  val op_sub  = io.alu_op(1)
  val op_slt  = io.alu_op(2)
  val op_sltu = io.alu_op(3)
  val op_and  = io.alu_op(4)
  val op_or   = io.alu_op(5)
  val op_xor  = io.alu_op(6)
  val op_sll  = io.alu_op(7)
  val op_srl  = io.alu_op(8)
  val op_sra  = io.alu_op(9)
  val op_lui  = io.alu_op(10)

  val sub_slt_sltu = op_sub | op_slt | op_sltu
  val adder_b = Mux(sub_slt_sltu, ~io.alu_src2, io.alu_src2)
  val adder_cin = sub_slt_sltu
  val adder_result_ext = io.alu_src1 +& adder_b + adder_cin
  val adder_result = adder_result_ext(31, 0)
  val adder_cout = adder_result_ext(32)

  val slt_res = Cat(0.U(31.W), (io.alu_src1(31) & ~io.alu_src2(31)) | (!(io.alu_src1(31) ^ io.alu_src2(31)) & adder_result(31)))
  val sltu_res = Cat(0.U(31.W), ~adder_cout)
  
  val shamt = io.alu_src2(4, 0)
  val sll_res = (io.alu_src1 << shamt)(31, 0)
  // sra/srl
  val sr_res = Mux(op_sra, (io.alu_src1.asSInt >> shamt).asUInt, io.alu_src1 >> shamt)(31, 0)

  io.alu_result := Mux1H(Seq(
    (op_add | op_sub) -> adder_result,
    op_slt            -> slt_res,
    op_sltu           -> sltu_res,
    op_and            -> (io.alu_src1 & io.alu_src2),
    op_or             -> (io.alu_src1 | io.alu_src2),
    op_xor            -> (io.alu_src1 ^ io.alu_src2),
    op_lui            -> io.alu_src2,
    op_sll            -> sll_res,
    (op_srl | op_sra) -> sr_res
  ))
}



// ======================================
// RegisterFile
// ======================================
class RegisterFile extends Module {
  val io = IO(new Bundle {
    val wdata  = Input(UInt(32.W))
    val waddr  = Input(UInt(5.W))
    val wen    = Input(Bool())
    val raddr1 = Input(UInt(5.W))
    val raddr2 = Input(UInt(5.W))
    val rdata1 = Output(UInt(32.W))
    val rdata2 = Output(UInt(32.W))
    val rf_dbg = Output(Vec(32, UInt(32.W)))
  })

  val rf = RegInit(VecInit(Seq.fill(32)(0.U(32.W))))
  when(io.wen && io.waddr =/= 0.U) {
    rf(io.waddr) := io.wdata
  }
  io.rdata1 := Mux(io.raddr1 === 0.U, 0.U, rf(io.raddr1))
  io.rdata2 := Mux(io.raddr2 === 0.U, 0.U, rf(io.raddr2))
  io.rf_dbg := rf
}