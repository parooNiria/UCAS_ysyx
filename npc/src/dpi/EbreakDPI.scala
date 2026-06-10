package npc

import chisel3._
import chisel3.util._

class EbreakDPI extends ExtModule {
  val io = FlatIO(new Bundle {
    val enable = Input(Bool())
    val dbg_reg_a0 = Input(UInt(32.W))
  })

  setInline("EbreakDPI.v",
    """
    |module EbreakDPI(
    |    input enable,
    |    input [31:0] dbg_reg_a0
    |);
    |  import "DPI-C" function void dpi_ebreak(int reg_a0);
    |  always @(*) begin
    |    if (enable) begin
    |      dpi_ebreak(dbg_reg_a0);
    |    end
    |  end
    |endmodule
    """.stripMargin)
}
