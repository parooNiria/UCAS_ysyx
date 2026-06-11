package npc

import chisel3._
import chisel3.util._

// ===========================================================================
// Performance Event DPI BlackBox
// All counting and analysis done in C++ via DPI-C.
// Called on every posedge clock. Report prints automatically at exit.
// ===========================================================================
class PerfEventDPI extends ExtModule {
  val io = FlatIO(new Bundle {
    val clk = Input(Clock())

    // IFU events
    val ifu_fetch      = Input(Bool())
    val ifu_stall_ar   = Input(Bool())
    val ifu_stall_r    = Input(Bool())
    val ifu_stall_bp   = Input(Bool())

    // IDU decode events (category pulses)
    val idu_compute    = Input(Bool())
    val idu_branch     = Input(Bool())
    val idu_jump       = Input(Bool())
    val idu_load       = Input(Bool())
    val idu_store      = Input(Bool())
    val idu_csr        = Input(Bool())
    val idu_system     = Input(Bool())

    // EXU events
    val exu_compute     = Input(Bool())
    val exu_load_issue  = Input(Bool())
    val exu_store_issue = Input(Bool())

    // LSU events
    val lsu_load_done  = Input(Bool())
    val lsu_store_done = Input(Bool())

    // WBU commit (for latency tracking dequeue)
    val wbu_commit     = Input(Bool())

    // ICache events
    val icache_access     = Input(Bool())
    val icache_hit        = Input(Bool())
    val icache_miss_cycle = Input(Bool())
  })

  setInline("PerfEventDPI.v",
    """
    |module PerfEventDPI(
    |    input         clk,
    |    input         ifu_fetch,
    |    input         ifu_stall_ar,
    |    input         ifu_stall_r,
    |    input         ifu_stall_bp,
    |    input         idu_compute,
    |    input         idu_branch,
    |    input         idu_jump,
    |    input         idu_load,
    |    input         idu_store,
    |    input         idu_csr,
    |    input         idu_system,
    |    input         exu_compute,
    |    input         exu_load_issue,
    |    input         exu_store_issue,
    |    input         lsu_load_done,
    |    input         lsu_store_done,
    |    input         wbu_commit,
    |    input         icache_access,
    |    input         icache_hit,
    |    input         icache_miss_cycle
    |);
    |
    |  import "DPI-C" function void dpi_perf_event(
    |    input int  ifu_fetch,
    |    input int  ifu_stall_ar,
    |    input int  ifu_stall_r,
    |    input int  ifu_stall_bp,
    |    input int  idu_compute,
    |    input int  idu_branch,
    |    input int  idu_jump,
    |    input int  idu_load,
    |    input int  idu_store,
    |    input int  idu_csr,
    |    input int  idu_system,
    |    input int  exu_compute,
    |    input int  exu_load_issue,
    |    input int  exu_store_issue,
    |    input int  lsu_load_done,
    |    input int  lsu_store_done,
    |    input int  wbu_commit,
    |    input int  icache_access,
    |    input int  icache_hit,
    |    input int  icache_miss_cycle
    |  );
    |
    |  always @(posedge clk) begin
    |    dpi_perf_event(
    |      {31'd0, ifu_fetch},
    |      {31'd0, ifu_stall_ar},
    |      {31'd0, ifu_stall_r},
    |      {31'd0, ifu_stall_bp},
    |      {31'd0, idu_compute},
    |      {31'd0, idu_branch},
    |      {31'd0, idu_jump},
    |      {31'd0, idu_load},
    |      {31'd0, idu_store},
    |      {31'd0, idu_csr},
    |      {31'd0, idu_system},
    |      {31'd0, exu_compute},
    |      {31'd0, exu_load_issue},
    |      {31'd0, exu_store_issue},
    |      {31'd0, lsu_load_done},
    |      {31'd0, lsu_store_done},
    |      {31'd0, wbu_commit},
    |      {31'd0, icache_access},
    |      {31'd0, icache_hit},
    |      {31'd0, icache_miss_cycle}
    |    );
    |  end
    |
    |endmodule
    """.stripMargin)
}
