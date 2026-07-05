`timescale 1ns/1ns

module tb_netlist;
  reg clk = 0, rst = 1;
  always #10 clk = ~clk;
  initial begin #50 rst = 0; end

  // master AXI buses
  wire [31:0] io_master_awaddr, io_master_wdata, io_master_araddr, io_master_rdata;
  wire [3:0]  io_master_awid, io_master_arid, io_master_wstrb, io_master_rid, io_master_bid;
  wire [7:0]  io_master_awlen, io_master_arlen;
  wire [2:0]  io_master_awsize, io_master_arsize;
  wire [1:0]  io_master_awburst, io_master_arburst, io_master_rresp, io_master_bresp;
  wire        io_master_awvalid, io_master_wvalid, io_master_wlast,
              io_master_awready, io_master_wready,
              io_master_arvalid, io_master_arready,
              io_master_rvalid, io_master_rlast, io_master_rready,
              io_master_bvalid, io_master_bready;

  axi_mem u_mem (
    .clk(clk), .rst(rst),
    .araddr(io_master_araddr),   .arid(io_master_arid),
    .arlen(io_master_arlen),     .arsize(io_master_arsize),
    .arburst(io_master_arburst), .arvalid(io_master_arvalid),
    .arready(io_master_arready),
    .rdata(io_master_rdata),     .rresp(io_master_rresp),
    .rlast(io_master_rlast),     .rid(io_master_rid),
    .rvalid(io_master_rvalid),   .rready(io_master_rready),
    .awaddr(io_master_awaddr),   .awid(io_master_awid),
    .awlen(io_master_awlen),     .awsize(io_master_awsize),
    .awburst(io_master_awburst), .awvalid(io_master_awvalid),
    .awready(io_master_awready),
    .wdata(io_master_wdata),     .wstrb(io_master_wstrb),
    .wlast(io_master_wlast),     .wvalid(io_master_wvalid),
    .wready(io_master_wready),
    .bresp(io_master_bresp),     .bid(io_master_bid),
    .bvalid(io_master_bvalid),   .bready(io_master_bready)
  );

  initial begin
    $readmemh(`BOOT_HEX, u_mem.boot_ram);
    $readmemh(`PROG_HEX, u_mem.prog_ram);
  end

  netlist_wrap u_dut (
    .clk(clk), .rst(rst), .io_interrupt(1'b0),
    .io_master_awaddr(io_master_awaddr),
    .io_master_awvalid(io_master_awvalid),
    .io_master_awready(io_master_awready),
    .io_master_wdata(io_master_wdata),
    .io_master_wvalid(io_master_wvalid),
    .io_master_wready(io_master_wready),
    .io_master_bvalid(io_master_bvalid),
    .io_master_bready(io_master_bready),
    .io_master_araddr(io_master_araddr),
    .io_master_arvalid(io_master_arvalid),
    .io_master_arready(io_master_arready),
    .io_master_rdata(io_master_rdata),
    .io_master_rvalid(io_master_rvalid),
    .io_master_rready(io_master_rready)
  );

  initial #10000000 $finish;
endmodule
