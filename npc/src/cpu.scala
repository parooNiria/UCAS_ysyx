package npc

import chisel3._
import chisel3.util._

class REFcommitDPI extends BlackBox with HasBlackBoxInline {
  val io = IO(new Bundle {
    val commit_enable = Input(Bool())
    val dbg_rf = Input(Vec(32, UInt(32.W)))
    val dbg_mstatus = Input(UInt(32.W))
    val dbg_mtvec = Input(UInt(32.W))
    val dbg_mepc = Input(UInt(32.W))
    val dbg_mcause = Input(UInt(32.W))
    val last_inst = Input(UInt(32.W))
    val next_pc = Input(UInt(32.W))
    val last_pc = Input(UInt(32.W))
    val device_type = Input(UInt(32.W))
  })
  // 直接内嵌 Verilog + DPI-C 调用
  setInline("REFcommitDPI.v",
    """
    module REFcommitDPI(
        input commit_enable,
        input [31:0] dbg_rf_0,
        input [31:0] dbg_rf_1,
        input [31:0] dbg_rf_2,
        input [31:0] dbg_rf_3,
        input [31:0] dbg_rf_4,
        input [31:0] dbg_rf_5,
        input [31:0] dbg_rf_6,
        input [31:0] dbg_rf_7,
        input [31:0] dbg_rf_8,
        input [31:0] dbg_rf_9,
        input [31:0] dbg_rf_10,
        input [31:0] dbg_rf_11,
        input [31:0] dbg_rf_12,
        input [31:0] dbg_rf_13,
        input [31:0] dbg_rf_14,
        input [31:0] dbg_rf_15,
        input [31:0] dbg_rf_16,
        input [31:0] dbg_rf_17,
        input [31:0] dbg_rf_18,
        input [31:0] dbg_rf_19,
        input [31:0] dbg_rf_20,
        input [31:0] dbg_rf_21,
        input [31:0] dbg_rf_22,
        input [31:0] dbg_rf_23,
        input [31:0] dbg_rf_24,
        input [31:0] dbg_rf_25,
        input [31:0] dbg_rf_26,
        input [31:0] dbg_rf_27,
        input [31:0] dbg_rf_28,
        input [31:0] dbg_rf_29,
        input [31:0] dbg_rf_30,
        input [31:0] dbg_rf_31,
        input [31:0] dbg_mstatus,
        input [31:0] dbg_mtvec,
        input [31:0] dbg_mepc,
        input [31:0] dbg_mcause,
        input [31:0] last_inst,
        input [31:0] next_pc,
        input [31:0] last_pc,
        input [31:0] device_type
    );

    import "DPI-C" function void dpi_commit(input int dbg_mstatus, 
    input int dbg_mtvec, input int dbg_mepc, input int dbg_mcause,
    input int dbg_rf0, 
    input int dbg_rf1, input int dbg_rf2, input int dbg_rf3, input int dbg_rf4, input int dbg_rf5,
    input int dbg_rf6, input int dbg_rf7, input int dbg_rf8, input int dbg_rf9, input int dbg_rf10, input int dbg_rf11, input int
    dbg_rf12, input int dbg_rf13, input int dbg_rf14, input int dbg_rf15, input int dbg_rf16, input int dbg_rf17,
    input int dbg_rf18, input int dbg_rf19, input int dbg_rf20, input int dbg_rf21, input int dbg_rf22, input int dbg_rf23, input int
    dbg_rf24, input int dbg_rf25, input int dbg_rf26, input int dbg_rf27, input int dbg_rf28, input int dbg_rf29, input int dbg_rf30, input int dbg_rf31, input int last_inst,
    input int next_pc, input int last_pc,input int device_type);
    always @(*) begin
      if (commit_enable) begin
          dpi_commit(dbg_mstatus, dbg_mtvec, dbg_mepc, dbg_mcause,
          dbg_rf_0, dbg_rf_1, dbg_rf_2, dbg_rf_3, dbg_rf_4, dbg_rf_5,
          dbg_rf_6, dbg_rf_7, dbg_rf_8, dbg_rf_9, dbg_rf_10, dbg_rf_11, dbg_rf_12, dbg_rf_13, dbg_rf_14, dbg_rf_15, dbg_rf_16, dbg_rf_17,
          dbg_rf_18, dbg_rf_19, dbg_rf_20, dbg_rf_21, dbg_rf_22, dbg_rf_23, dbg_rf_24, dbg_rf_25, dbg_rf_26, dbg_rf_27, dbg_rf_28, dbg_rf_29, dbg_rf_30, dbg_rf_31,
          last_inst, next_pc, last_pc, device_type);
      end
    end

    endmodule
    """.stripMargin)
}
  
class REFinitDPI extends BlackBox with HasBlackBoxInline {
  val io = IO(new Bundle {
    val init_enable = Input(Bool())
    val dbg_mstatus = Input(UInt(32.W))
    val dbg_mtvec = Input(UInt(32.W))
    val dbg_mepc = Input(UInt(32.W))
    val dbg_mcause = Input(UInt(32.W))
    val dbg_pc = Input(UInt(32.W))
  })
  // 直接内嵌 Verilog + DPI-C 调用
  setInline("REFinitDPI.v",
    """
    module REFinitDPI(
        input init_enable,
        input [31:0] dbg_mstatus,
        input [31:0] dbg_mtvec,
        input [31:0] dbg_mepc,
        input [31:0] dbg_mcause,
        input [31:0] dbg_pc
    );

    import "DPI-C" function void dpi_init(input int dbg_mstatus, 
    input int dbg_mtvec, input int dbg_mepc, input int dbg_mcause, input int dbg_pc);
    always @(*) begin
      if (init_enable) begin
          dpi_init(dbg_mstatus, dbg_mtvec, dbg_mepc, dbg_mcause, dbg_pc);
      end
    end

    endmodule
    """.stripMargin)
}


class cpu extends Module {
  val io = IO(new Bundle {
    val axi_if = new AXI4Bundle
    val axi_mem = new AXI4Bundle
    val interrupt = Input(Bool())
  })

  val ifu = Module(new IFU)
  val idu = Module(new IDU)
  val exu = Module(new EXU)
  val memu = Module(new MEMU)
  val wbu = Module(new WBU)
  val rf = Module(new RegisterFile)

  ifu.io.commit_info.commit_valid := wbu.io.out.commit_valid
  ifu.io.commit_info.next_pc := wbu.io.out.next_pc

  idu.io.in <> ifu.io.out
  exu.io.in <> idu.io.out
  memu.io.in <> exu.io.out
  wbu.io.in <> memu.io.out

  rf.io.raddr1 := idu.io.rf_read.raddr1
  rf.io.raddr2 := idu.io.rf_read.raddr2
  idu.io.rf_read.rdata1 := rf.io.rdata1
  idu.io.rf_read.rdata2 := rf.io.rdata2

  rf.io.wen := wbu.io.out.reg_we_en && wbu.io.out.commit_valid
  rf.io.waddr := wbu.io.out.reg_dest
  rf.io.wdata := wbu.io.out.reg_write_data

  io.axi_if <> ifu.io.if_axi

  io.axi_mem.awaddr := exu.io.awaddr
  io.axi_mem.awvalid := exu.io.awvalid
  exu.io.awready := io.axi_mem.awready
  io.axi_mem.awid := exu.io.awid
  io.axi_mem.awlen := exu.io.awlen
  io.axi_mem.awsize := exu.io.awsize
  io.axi_mem.awburst := exu.io.awburst

  io.axi_mem.wdata := exu.io.wdata
  io.axi_mem.wstrb := exu.io.wstrb
  io.axi_mem.wvalid := exu.io.wvalid
  exu.io.wready := io.axi_mem.wready
  io.axi_mem.wlast := exu.io.wlast

  io.axi_mem.araddr := exu.io.araddr
  io.axi_mem.arvalid := exu.io.arvalid
  exu.io.arready := io.axi_mem.arready
  io.axi_mem.arid := exu.io.arid
  io.axi_mem.arlen := exu.io.arlen
  io.axi_mem.arsize := exu.io.arsize
  io.axi_mem.arburst := exu.io.arburst

  memu.io.rdata := io.axi_mem.rdata
  memu.io.rresp := io.axi_mem.rresp
  memu.io.rvalid := io.axi_mem.rvalid
  memu.io.rlast := io.axi_mem.rlast
  memu.io.rid := io.axi_mem.rid
  io.axi_mem.rready := memu.io.rready

  memu.io.bresp := io.axi_mem.bresp
  memu.io.bvalid := io.axi_mem.bvalid
  memu.io.bid := io.axi_mem.bid
  io.axi_mem.bready := memu.io.bready
  wbu.io.dbg_reg_a0 := rf.io.rf_dbg(10)
  val commit_last = RegInit(false.B)
  val last_inst = Reg(UInt(32.W))
  val last_pc = Reg(UInt(32.W))
  val last_next_pc = Reg(UInt(32.W))
  when(wbu.io.out.commit_valid) {
    commit_last := true.B
    last_inst := wbu.io.out.inst
    last_pc := wbu.io.out.pc
    last_next_pc := wbu.io.out.next_pc
  } .otherwise {
    commit_last := false.B
    last_inst := 0.U
    last_pc := 0.U
    last_next_pc := 0.U
  }

  val REFcommitDPI = Module(new REFcommitDPI)
  REFcommitDPI.io.commit_enable := commit_last
  REFcommitDPI.io.dbg_rf := rf.io.rf_dbg
  REFcommitDPI.io.dbg_mstatus := wbu.io.dbg_mstatus
  REFcommitDPI.io.dbg_mtvec := wbu.io.dbg_mtvec
  REFcommitDPI.io.dbg_mepc := wbu.io.dbg_mepc
  REFcommitDPI.io.dbg_mcause := wbu.io.dbg_mcause
  REFcommitDPI.io.last_inst := last_inst
  REFcommitDPI.io.next_pc := last_next_pc
  REFcommitDPI.io.last_pc := last_pc
  REFcommitDPI.io.device_type := Cat(0.U(31.W), wbu.io.out.device_access)

val rst = this.reset.asBool

val rst_delayed = RegNext(rst)

val init_pulse = rst_delayed && !rst

  val REFinitDPI = Module(new REFinitDPI)
  REFinitDPI.io.init_enable := init_pulse
  REFinitDPI.io.dbg_mstatus := wbu.io.dbg_mstatus
  REFinitDPI.io.dbg_mtvec := wbu.io.dbg_mtvec
  REFinitDPI.io.dbg_mepc := wbu.io.dbg_mepc
  REFinitDPI.io.dbg_mcause := wbu.io.dbg_mcause
  REFinitDPI.io.dbg_pc := ifu.io.out.bits.pc
}
