package npc

import chisel3._
import chisel3.util._
class EXU extends Module {
    val io = IO(new Bundle {
        val in = Flipped(Decoupled(new MessageID))
        val out = Decoupled(new MessageEXE)
        
        val awaddr  = Output(UInt(32.W))
        val awvalid = Output(Bool())
        val awready = Input(Bool())

        val wdata  = Output(UInt(32.W))
        val wstrb  = Output(UInt(4.W))
        val wvalid = Output(Bool())
        val wready = Input(Bool())

        val araddr  = Output(UInt(32.W))
        val arvalid = Output(Bool())
        val arready = Input(Bool())
    })
    val instReg = Reg(UInt(32.W))
    val pcReg = Reg(UInt(32.W))
    val nextBranchPcReg = Reg(UInt(32.W))
    val aluOpReg = Reg(UInt(11.W))
    val aluSrc1Reg = Reg(UInt(32.W))
    val aluSrc2Reg = Reg(UInt(32.W))
    val writeDataReg = Reg(UInt(32.W))
    val regCsrMemEnDestReg = Reg(UInt(8.W))
    val memEnLSReg = Reg(UInt(5.W))
    val sysMessageReg = Reg(UInt(3.W))

    val validReg = RegInit(false.B)
    when(io.in.fire) {
      instReg := io.in.bits.inst
      pcReg := io.in.bits.pc
      nextBranchPcReg := io.in.bits.next_branch_pc
      aluOpReg := io.in.bits.alu_op
      aluSrc1Reg := io.in.bits.alu_src1
      aluSrc2Reg := io.in.bits.alu_src2
      writeDataReg := io.in.bits.write_data
      regCsrMemEnDestReg := io.in.bits.reg_csr_mem_en_dest
      memEnLSReg := io.in.bits.mem_en_LS_Type
      sysMessageReg := io.in.bits.sys_message
      validReg := true.B
    }

    val alu = Module(new ALU)
    alu.io.alu_op := aluOpReg
    alu.io.alu_src1 := aluSrc1Reg
    alu.io.alu_src2 := aluSrc2Reg

    val memEn = memEnLSReg(4)
    val isLoad = memEnLSReg(3)
    val isStore = memEn && !isLoad
    val func3 = memEnLSReg(2, 0)

    val addr = Cat(alu.io.alu_result(31, 2), 0.U(2.W))
    val addrLow = alu.io.alu_result(1, 0)
    val sbMask = MuxLookup(addrLow, "b1000".U(8.W))(Seq(
      "b00".U -> "b0001".U(8.W),
      "b01".U -> "b0010".U(8.W),
      "b10".U -> "b0100".U(8.W)
    ))
    val shMask = Mux(alu.io.alu_result(1), "b1100".U(8.W), "b0011".U(8.W))
    val wmaskVal = Mux(func3 === "b000".U, sbMask,
      Mux(func3 === "b001".U, shMask,
      Mux(func3 === "b010".U, "b1111".U(8.W), "b0000".U(8.W))))
    val write_data_byte = Fill(4, writeDataReg(7, 0))
    val write_data_half = Fill(2, writeDataReg(15, 0))
    val write_data = Mux(func3 === "b000".U,  write_data_byte,
                    Mux(func3 === "b001".U,  write_data_half,
                    Mux(func3 === "b010".U, writeDataReg, 0.U)))
    io.awaddr := addr
    io.awvalid := validReg && isStore
    io.wdata := write_data
    io.wstrb := wmaskVal
    io.wvalid := validReg && isStore
    io.araddr := addr
    io.arvalid := validReg && isLoad
    val device_addr_in = (addr >= "hA00003F8".U && addr < "hA0000400".U)||
                          (addr >= "ha0000048".U && addr < "ha0000050".U) 
    val is_device_access = validReg && (isLoad || isStore) && (device_addr_in) 
    io.out.bits.device_access := is_device_access

    io.out.valid := validReg
    io.out.bits.inst := instReg
    io.out.bits.pc := pcReg
    io.out.bits.next_branch_pc := nextBranchPcReg
    io.out.bits.alu_result := alu.io.alu_result
    io.out.bits.write_data_csr := writeDataReg
    io.out.bits.mem_en_LS_Type := memEnLSReg
    io.out.bits.reg_csr_mem_en_dest := regCsrMemEnDestReg
    io.out.bits.sys_message := sysMessageReg

    when(io.out.fire) {
      validReg := false.B
    }

    io.in.ready := !validReg || io.out.fire
}   