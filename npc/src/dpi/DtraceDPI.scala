package npc

import chisel3._
import chisel3.util._

// ===========================================================================
// Data Trace DPI BlackBox  (dtrace)
// Captures load/store addresses/sizes from EXU for offline cache simulation.
// Called on every posedge clock; C++ side writes to npc-log-dtrace.txt.
// ===========================================================================
class DtraceDPI extends ExtModule {
  val io = FlatIO(new Bundle {
    val clk          = Input(Clock())

    val load_valid   = Input(Bool())   // pulse: load instruction completed in EXU
    val store_valid  = Input(Bool())   // pulse: store instruction completed in EXU

    val addr         = Input(UInt(32.W))  // byte address of the access
    val mem_size     = Input(UInt(3.W))   // func3[2:0]: 0=byte, 1=half, 2=word
    val wdata        = Input(UInt(32.W))  // store data (valid when store_valid)
    val wstrb        = Input(UInt(4.W))   // write strobe mask (valid when store_valid)
  })

  setInline("DtraceDPI.v",
    """
    |module DtraceDPI(
    |    input         clk,
    |    input         load_valid,
    |    input         store_valid,
    |    input  [31:0] addr,
    |    input  [ 2:0] mem_size,
    |    input  [31:0] wdata,
    |    input  [ 3:0] wstrb
    |);
    |
    |  import "DPI-C" function void dpi_dtrace_event(
    |    input int  load_valid,
    |    input int  store_valid,
    |    input int  addr,
    |    input int  mem_size,
    |    input int  wdata,
    |    input int  wstrb
    |  );
    |
    |  always @(posedge clk) begin
    |    dpi_dtrace_event(
    |      {31'd0, load_valid},
    |      {31'd0, store_valid},
    |      addr,
    |      {29'd0, mem_size},
    |      wdata,
    |      {28'd0, wstrb}
    |    );
    |  end
    |
    |endmodule
    """.stripMargin)
}
