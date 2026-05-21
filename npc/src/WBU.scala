package npc

import chisel3._
import chisel3.util._

class EbreakDPI extends BlackBox with HasBlackBoxInline {
  val io = IO(new Bundle {
    val enable = Input(Bool())  // 触发信号 = ebreak
    val dbg_reg_a0 = Input(UInt(32.W))  // 传递 reg_a0 的值
  })

  // 直接内嵌 Verilog + DPI-C 调用
  setInline("EbreakDPI.v",
    """
    module EbreakDPI(
        input enable,
        input [31:0] dbg_reg_a0
    );
    import "DPI-C" function void dpi_ebreak(int reg_a0);
    always @(*) begin
    if (enable) begin
        dpi_ebreak(dbg_reg_a0);  // ebreak 时调用 C 函数
    end
    end
    endmodule
    """.stripMargin)
}

class WBU extends Module {
    val io = IO(new Bundle {
        val in = Flipped(Decoupled(new MessageMEM))
        val out = new CommitInfo
        val dbg_mstatus = Output(UInt(32.W))
        val dbg_mtvec   = Output(UInt(32.W))
        val dbg_mepc    = Output(UInt(32.W))
        val dbg_mcause  = Output(UInt(32.W))
        val dbg_reg_a0  = Input(UInt(32.W))
    })

    val csr = Module(new CSR)
    val handshake = io.in.valid && io.in.ready
    val valid = RegInit(false.B)
    when(handshake) {
        valid := true.B
    } .otherwise {
        valid := false.B
    }
    val inst_reg = Reg(UInt(32.W))
    val pc_reg = Reg(UInt(32.W))
    val next_branch_pc_reg = Reg(UInt(32.W))
    val reg_write_data_reg = Reg(UInt(32.W))
    val csr_write_data_reg = Reg(UInt(32.W))
    val reg_csr_en_dest_reg = Reg(UInt(7.W))
    val sys_message_reg = Reg(UInt(3.W))
    val device_access_reg = Reg(Bool())
    when(handshake) {
        inst_reg := io.in.bits.inst
        pc_reg := io.in.bits.pc
        next_branch_pc_reg := io.in.bits.next_branch_pc
        reg_write_data_reg := io.in.bits.reg_write_data
        csr_write_data_reg := io.in.bits.csr_write_data
        reg_csr_en_dest_reg := io.in.bits.reg_csr_en_dest
        sys_message_reg := io.in.bits.sys_message
        device_access_reg := io.in.bits.device_access
    }

    val instMret = sys_message_reg(0)
    val instEcall = sys_message_reg(2)
    val instEbreak = sys_message_reg(1)
    val func3 = io.in.bits.inst(14, 12)
    csr.io.csr_waddr := inst_reg(31, 20)
    csr.io.csr_wdata := Mux(func3 === "b011".U || func3 === "b111".U , 0.U,csr_write_data_reg)
    csr.io.csr_wmask := Mux(func3 === "b001".U || func3 === "b101".U, "hffffffff".U(32.W), csr_write_data_reg)
    csr.io.csr_we := valid && reg_csr_en_dest_reg(6)
    csr.io.csr_raddr := inst_reg(31, 20)
    csr.io.ex := (instEcall || instEbreak)&&valid
    csr.io.mret := instMret&&valid
    csr.io.epc := pc_reg
    csr.io.cause := Mux(instEcall, 11.U, 3.U)

    io.dbg_mstatus := csr.io.mstatus_val
    io.dbg_mtvec := csr.io.mtvec_val
    io.dbg_mepc := csr.io.mepc_val
    io.dbg_mcause := csr.io.mcause_val

    io.in.ready := true.B
    io.out.inst := inst_reg
    io.out.pc := pc_reg
    io.out.next_pc := Mux(instMret, csr.io.mepc_val,
      Mux(instEcall || instEbreak, csr.io.mtvec_val, next_branch_pc_reg))
    io.out.reg_dest := reg_csr_en_dest_reg(4, 0)
    io.out.reg_we_en := reg_csr_en_dest_reg(5)
    io.out.reg_write_data := Mux(reg_csr_en_dest_reg(6), csr.io.csr_rdata, reg_write_data_reg)
    io.out.commit_valid := valid
    io.out.device_access := device_access_reg
    io.out.ebreak := instEbreak && valid
    val dpi_ebreak = Module(new EbreakDPI)
    dpi_ebreak.io.enable := io.out.ebreak  // 触发 ebreak 时调用 C 函数
    dpi_ebreak.io.dbg_reg_a0 := io.dbg_reg_a0  // 将 reg_a0 的值传递给 DPI 模块
}