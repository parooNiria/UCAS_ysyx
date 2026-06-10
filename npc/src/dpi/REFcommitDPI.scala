package npc

import chisel3._
import chisel3.util._

class REFcommitDPI extends ExtModule {
  val io = FlatIO(new Bundle {
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

  setInline("REFcommitDPI.v",
    """
    |module REFcommitDPI(
    |    input commit_enable,
    |    input [31:0] dbg_rf_0,
    |    input [31:0] dbg_rf_1,
    |    input [31:0] dbg_rf_2,
    |    input [31:0] dbg_rf_3,
    |    input [31:0] dbg_rf_4,
    |    input [31:0] dbg_rf_5,
    |    input [31:0] dbg_rf_6,
    |    input [31:0] dbg_rf_7,
    |    input [31:0] dbg_rf_8,
    |    input [31:0] dbg_rf_9,
    |    input [31:0] dbg_rf_10,
    |    input [31:0] dbg_rf_11,
    |    input [31:0] dbg_rf_12,
    |    input [31:0] dbg_rf_13,
    |    input [31:0] dbg_rf_14,
    |    input [31:0] dbg_rf_15,
    |    input [31:0] dbg_rf_16,
    |    input [31:0] dbg_rf_17,
    |    input [31:0] dbg_rf_18,
    |    input [31:0] dbg_rf_19,
    |    input [31:0] dbg_rf_20,
    |    input [31:0] dbg_rf_21,
    |    input [31:0] dbg_rf_22,
    |    input [31:0] dbg_rf_23,
    |    input [31:0] dbg_rf_24,
    |    input [31:0] dbg_rf_25,
    |    input [31:0] dbg_rf_26,
    |    input [31:0] dbg_rf_27,
    |    input [31:0] dbg_rf_28,
    |    input [31:0] dbg_rf_29,
    |    input [31:0] dbg_rf_30,
    |    input [31:0] dbg_rf_31,
    |    input [31:0] dbg_mstatus,
    |    input [31:0] dbg_mtvec,
    |    input [31:0] dbg_mepc,
    |    input [31:0] dbg_mcause,
    |    input [31:0] last_inst,
    |    input [31:0] next_pc,
    |    input [31:0] last_pc,
    |    input [31:0] device_type
    |);
    |
    |  import "DPI-C" function void dpi_commit(
    |    input int dbg_mstatus, input int dbg_mtvec,
    |    input int dbg_mepc, input int dbg_mcause,
    |    input int dbg_rf0, input int dbg_rf1, input int dbg_rf2,
    |    input int dbg_rf3, input int dbg_rf4, input int dbg_rf5,
    |    input int dbg_rf6, input int dbg_rf7, input int dbg_rf8,
    |    input int dbg_rf9, input int dbg_rf10, input int dbg_rf11,
    |    input int dbg_rf12, input int dbg_rf13, input int dbg_rf14,
    |    input int dbg_rf15, input int dbg_rf16, input int dbg_rf17,
    |    input int dbg_rf18, input int dbg_rf19, input int dbg_rf20,
    |    input int dbg_rf21, input int dbg_rf22, input int dbg_rf23,
    |    input int dbg_rf24, input int dbg_rf25, input int dbg_rf26,
    |    input int dbg_rf27, input int dbg_rf28, input int dbg_rf29,
    |    input int dbg_rf30, input int dbg_rf31,
    |    input int last_inst, input int next_pc,
    |    input int last_pc, input int device_type
    |  );
    |
    |  always @(*) begin
    |    if (commit_enable) begin
    |      dpi_commit(dbg_mstatus, dbg_mtvec, dbg_mepc, dbg_mcause,
    |        dbg_rf_0, dbg_rf_1, dbg_rf_2, dbg_rf_3, dbg_rf_4, dbg_rf_5,
    |        dbg_rf_6, dbg_rf_7, dbg_rf_8, dbg_rf_9, dbg_rf_10, dbg_rf_11,
    |        dbg_rf_12, dbg_rf_13, dbg_rf_14, dbg_rf_15, dbg_rf_16, dbg_rf_17,
    |        dbg_rf_18, dbg_rf_19, dbg_rf_20, dbg_rf_21, dbg_rf_22, dbg_rf_23,
    |        dbg_rf_24, dbg_rf_25, dbg_rf_26, dbg_rf_27, dbg_rf_28, dbg_rf_29,
    |        dbg_rf_30, dbg_rf_31,
    |        last_inst, next_pc, last_pc, device_type);
    |    end
    |  end
    |endmodule
    """.stripMargin)
}