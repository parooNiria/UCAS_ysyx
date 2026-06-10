package npc

import chisel3._
import chisel3.util._

class REFinitDPI extends ExtModule {
  val io = FlatIO(new Bundle {
    val init_enable = Input(Bool())
    val dbg_mstatus = Input(UInt(32.W))
    val dbg_mtvec = Input(UInt(32.W))
    val dbg_mepc = Input(UInt(32.W))
    val dbg_mcause = Input(UInt(32.W))
    val dbg_pc = Input(UInt(32.W))
  })

  setInline("REFinitDPI.v",
    """
    |module REFinitDPI(
    |    input init_enable,
    |    input [31:0] dbg_mstatus,
    |    input [31:0] dbg_mtvec,
    |    input [31:0] dbg_mepc,
    |    input [31:0] dbg_mcause,
    |    input [31:0] dbg_pc
    |);
    |
    |  import "DPI-C" function void dpi_init(
    |    input int dbg_mstatus, input int dbg_mtvec,
    |    input int dbg_mepc, input int dbg_mcause,
    |    input int dbg_pc
    |  );
    |
    |  always @(*) begin
    |    if (init_enable) begin
    |      dpi_init(dbg_mstatus, dbg_mtvec, dbg_mepc, dbg_mcause, dbg_pc);
    |    end
    |  end
    |endmodule
    """.stripMargin)
}
