`timescale 1ns/1ns

module tb_sim;
  reg clk = 0, rst = 1;
  always #10 clk = ~clk;
  initial begin #50 rst = 0; end

  // AXI master — wires
  wire [31:0] m_awaddr, m_wdata, m_araddr, m_rdata;
  wire [3:0]  m_awid, m_arid, m_wstrb, m_rid, m_bid;
  wire [7:0]  m_awlen, m_arlen;
  wire [2:0]  m_awsize, m_arsize;
  wire [1:0]  m_awburst, m_arburst, m_rresp, m_bresp;
  wire        m_awvalid, m_wvalid, m_wlast, m_awready, m_wready;
  wire        m_arvalid, m_arready, m_rvalid, m_rlast, m_rready;
  wire        m_bvalid, m_bready;

  axi_mem u_mem (
    .clk(clk), .rst(rst),
    .araddr(m_araddr), .arid(m_arid), .arlen(m_arlen),
    .arsize(m_arsize), .arburst(m_arburst), .arvalid(m_arvalid),
    .arready(m_arready), .rdata(m_rdata), .rresp(m_rresp),
    .rlast(m_rlast), .rid(m_rid), .rvalid(m_rvalid), .rready(m_rready),
    .awaddr(m_awaddr), .awid(m_awid), .awlen(m_awlen),
    .awsize(m_awsize), .awburst(m_awburst), .awvalid(m_awvalid),
    .awready(m_awready), .wdata(m_wdata), .wstrb(m_wstrb),
    .wlast(m_wlast), .wvalid(m_wvalid), .wready(m_wready),
    .bresp(m_bresp), .bid(m_bid), .bvalid(m_bvalid), .bready(m_bready)
  );

  initial begin
    $readmemh(`BOOT_HEX, u_mem.boot_ram);
    $readmemh(`PROG_HEX, u_mem.prog_ram);
  end

  ysyx_26060177 u_dut (
    .clock(clk), .reset(rst), .io_interrupt(1'b0),
    .io_master_awaddr(m_awaddr), .io_master_awid(m_awid),
    .io_master_awlen(m_awlen), .io_master_awsize(m_awsize),
    .io_master_awburst(m_awburst), .io_master_awvalid(m_awvalid),
    .io_master_awready(m_awready),
    .io_master_wdata(m_wdata), .io_master_wstrb(m_wstrb),
    .io_master_wlast(m_wlast), .io_master_wvalid(m_wvalid),
    .io_master_wready(m_wready),
    .io_master_bresp(m_bresp), .io_master_bid(m_bid),
    .io_master_bvalid(m_bvalid), .io_master_bready(m_bready),
    .io_master_araddr(m_araddr), .io_master_arid(m_arid),
    .io_master_arlen(m_arlen), .io_master_arsize(m_arsize),
    .io_master_arburst(m_arburst), .io_master_arvalid(m_arvalid),
    .io_master_arready(m_arready),
    .io_master_rdata(m_rdata), .io_master_rresp(m_rresp),
    .io_master_rlast(m_rlast), .io_master_rid(m_rid),
    .io_master_rvalid(m_rvalid), .io_master_rready(m_rready)
    // io_slave_* left unconnected (tied off internally)
  );

  // debug: count reads
  integer rd_cnt = 0;
  always @(posedge clk) if (m_arvalid && m_arready) begin
    rd_cnt <= rd_cnt + 1;
    if (rd_cnt < 50)
      $display("[%0t] rd#%0d pc=0x%08h inst=0x%08h", $time, rd_cnt, m_araddr, m_rdata);
  end

  initial #10000000 $finish;
endmodule
