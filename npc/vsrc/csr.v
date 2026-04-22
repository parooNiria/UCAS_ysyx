module csr #(
    parameter MARCHID = 32'h150be98,
    parameter MVENDORID = 32'h79737978
)(
    input wire clk,
    input wire rst,
    //csr读写信息
    input wire csr_we,
    input wire [11:0] csr_waddr,
    input wire [31:0] csr_wmask,
    input wire [31:0] csr_wdata,
    
    input wire [11:0] csr_raddr,
    output wire [31:0] csr_rdata,

    //特权相关信息
    input wire ex,
    input wire [31:0] epc,
    input wire [31:0] cause,
    input wire mret,

    output wire [31:0] mtvec_val,
    output wire [31:0] mepc_val
);

`define CSR_MVENDORID 12'hF11
`define CSR_MARCHID   12'hF12
`define CSR_MSTATUS   12'h300
`define CSR_MTVEC     12'h305
`define CSR_MEPC      12'h341
`define CSR_MCAUSE    12'h342
`define CSR_MCYCLE    12'hB00
`define CSR_MCYCLEH   12'hB80

`define CSR_MSTATUS_MIE 3
`define CSR_MSTATUS_MPIE 7
`define CSR_MSTATUS_MPP 12:11
  
reg [63:0] mcycle;
reg [31:0] mstatus;
reg [31:0] mvendorid;
reg [31:0] marchid;
reg [31:0] mtvec;
reg [31:0] mepc;
reg [31:0] mcause;

wire [31:0] mcycle_low = mcycle[31:0];
wire [31:0] mcycle_high = mcycle[63:32];

assign mtvec_val = mtvec;
assign mepc_val  = mepc;

always @(posedge clk) begin
    if (rst) begin
        mcycle <= 64'b0;
    end else begin
        mcycle <= mcycle + 1'b1;
    end
end

always @(posedge clk) begin
    if(rst) begin
        mvendorid <= MVENDORID;
        marchid   <= MARCHID;
    end
end

always @(posedge clk) begin
    if(rst)
        mstatus   <= 32'h1800; // MPP = 2'b11 (M-mode)
    else if (mret) begin
        mstatus[`CSR_MSTATUS_MIE] <= mstatus[`CSR_MSTATUS_MPIE];
        mstatus[`CSR_MSTATUS_MPIE] <= 1'b1; 
        mstatus[`CSR_MSTATUS_MPP] <= 2'b00; // MPP 保持 M-mode
    end else if (ex) begin
        mstatus[`CSR_MSTATUS_MPIE] <= mstatus[`CSR_MSTATUS_MIE];
        mstatus[`CSR_MSTATUS_MIE] <= 1'b0; 
        mstatus[`CSR_MSTATUS_MPP] <= 2'b11; // MPP 保持 M-mode
    end else if (csr_we && csr_waddr == `CSR_MSTATUS) begin
        mstatus <= (mstatus & ~csr_wmask) | (csr_wdata & csr_wmask);
    end
end

always @(posedge clk) begin
    if(rst) 
        mepc <= 0;
    else if (ex) 
        mepc <= epc;
    else if (csr_we && csr_waddr == `CSR_MEPC) 
        mepc <= (mepc & ~csr_wmask) | (csr_wdata & csr_wmask);
end

always @(posedge clk) begin
    if(rst) 
        mcause <= 0;
    else if (ex) 
        mcause <= cause;
    else if (csr_we && csr_waddr == `CSR_MCAUSE) 
        mcause <= (mcause & ~csr_wmask) | (csr_wdata & csr_wmask);
end

always @(posedge clk) begin
    if(rst) 
        mtvec <= 0;
    else if (csr_we && csr_waddr == `CSR_MTVEC) 
        mtvec <= (mtvec & ~csr_wmask) | (csr_wdata & csr_wmask);
end

assign csr_rdata =  ({32{csr_raddr == `CSR_MVENDORID}} & mvendorid) |
                    ({32{csr_raddr == `CSR_MARCHID}}   & marchid) |
                    ({32{csr_raddr == `CSR_MSTATUS}}   & mstatus) |
                    ({32{csr_raddr == `CSR_MTVEC}}     & mtvec) |
                    ({32{csr_raddr == `CSR_MEPC}}      & mepc) |
                    ({32{csr_raddr == `CSR_MCAUSE}}    & mcause) |
                    ({32{csr_raddr == `CSR_MCYCLE}}    & mcycle_low) |
                    ({32{csr_raddr == `CSR_MCYCLEH}}   & mcycle_high);

endmodule
