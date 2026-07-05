// AXI4 behavioral memory for iverilog simulation
// Supports burst reads (arlen > 0) and single-beat writes.
// Byte-wide RAMs (compatible with objcopy -O verilog $readmemh)
module axi_mem (
  input  clk, rst,
  input  [31:0] araddr,  input [3:0] arid,    input [7:0] arlen,
  input  [2:0]  arsize,  input [1:0] arburst, input arvalid,
  output reg    arready,
  output reg [31:0] rdata, output reg [1:0] rresp,
  output reg        rlast, output reg [3:0] rid,
  output reg        rvalid, input rready,
  input  [31:0] awaddr,  input [3:0] awid,    input [7:0] awlen,
  input  [2:0]  awsize,  input [1:0] awburst, input awvalid,
  output reg    awready,
  input  [31:0] wdata,   input [3:0] wstrb,   input wlast,
  input         wvalid,  output reg wready,
  output reg [1:0] bresp, output reg [3:0] bid,
  output reg       bvalid, input bready
);

  reg [7:0] boot_ram [0:4095];
  reg [7:0] prog_ram [0:8388607];

  wire is_boot_rd = (araddr >= 32'h30000000) && (araddr < 32'h30001000);
  wire is_boot_wr = (awaddr >= 32'h30000000) && (awaddr < 32'h30001000);
  wire is_prog_rd = (araddr >= 32'h80000000) && (araddr < 32'h80800000);
  wire is_prog_wr = (awaddr >= 32'h80000000) && (awaddr < 32'h80800000);

  // burst tracking
  reg  rd_busy;         // read burst in progress
  reg  [7:0] rd_cnt;    // beats remaining-1 (like arlen)
  reg  [31:0] rd_addr;  // current burst address
  reg  [3:0]  rd_id;
  wire        rd_is_boot, rd_is_prog;

  assign rd_is_boot = (rd_addr >= 32'h30000000) && (rd_addr < 32'h30001000);
  assign rd_is_prog = (rd_addr >= 32'h80000000) && (rd_addr < 32'h80800000);

  function [31:0] read32;
    input is_boot, is_prog;
    input [31:0] addr;
    begin
      if (is_boot) read32 = { boot_ram[addr-32'h30000000+3], boot_ram[addr-32'h30000000+2],
                              boot_ram[addr-32'h30000000+1], boot_ram[addr-32'h30000000+0] };
      else read32 = { prog_ram[addr-32'h80000000+3], prog_ram[addr-32'h80000000+2],
                      prog_ram[addr-32'h80000000+1], prog_ram[addr-32'h80000000+0] };
    end
  endfunction

  always @(posedge clk) begin
    if (rst) begin
      arready <= 0; rvalid <= 0; awready <= 0; wready <= 0; bvalid <= 0;
      rd_busy <= 0; rd_cnt <= 0;
      rid <= 0; rresp <= 0; rlast <= 0; bid <= 0; bresp <= 0;
    end
    else begin
      bvalid <= 0;
      // default: keep burst going (rready handshake deasserts rvalid)
      if (rvalid && rready) rvalid <= 0;

      // ── accept new read request ──
      arready <= !rd_busy;
      if (arvalid && !rd_busy) begin
        rd_busy  <= 1;
        rd_cnt   <= arlen;
        rd_addr  <= araddr;
        rd_id    <= arid;
        arready  <= 0;
      end

      // ── burst read data ──
      if (rd_busy && !rvalid) begin
        rid   <= rd_id;  rresp <= 0;
        if (rd_cnt == 0) rlast <= 1; else rlast <= 0;
        if (rd_is_boot)
          rdata <= read32(1'b1, 1'b0, rd_addr);
        else if (rd_is_prog)
          rdata <= read32(1'b0, 1'b1, rd_addr);
        else
          rdata <= 32'h00000013;
        rvalid <= 1;
        // advance
        if (rd_cnt == 0) begin
          rd_busy <= 0;
        end else begin
          rd_cnt  <= rd_cnt - 1;
          rd_addr <= rd_addr + 4;
        end
      end

      // ── write (single-beat) ──
      awready <= awvalid;
      wready  <= awvalid;
      if (awvalid && wvalid && wlast) begin
        bvalid <= 1; bresp <= 0; bid <= awid;
        if (is_boot_wr) begin
          if (wstrb[0]) boot_ram[awaddr-32'h30000000+0] <= wdata[ 7: 0];
          if (wstrb[1]) boot_ram[awaddr-32'h30000000+1] <= wdata[15: 8];
          if (wstrb[2]) boot_ram[awaddr-32'h30000000+2] <= wdata[23:16];
          if (wstrb[3]) boot_ram[awaddr-32'h30000000+3] <= wdata[31:24];
        end else if (is_prog_wr) begin
          if (wstrb[0]) prog_ram[awaddr-32'h80000000+0] <= wdata[ 7: 0];
          if (wstrb[1]) prog_ram[awaddr-32'h80000000+1] <= wdata[15: 8];
          if (wstrb[2]) prog_ram[awaddr-32'h80000000+2] <= wdata[23:16];
          if (wstrb[3]) prog_ram[awaddr-32'h80000000+3] <= wdata[31:24];
        end
      end
    end
  end

endmodule
