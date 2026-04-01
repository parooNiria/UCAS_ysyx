module top #(
    parameter RESET_PC = 32'h80000000
)(
    input         clk,
    input         rst,
    output reg [31:0] pc,
    input  [31:0] inst,
    output [31:0] ram_addr,
    output        ram_ren,
    input  [31:0] ram_rdata
);  
    import "DPI-C" function void ebreak_notify();
    import "DPI-C" function void pmem_write(input int waddr, input int wdata, input byte wmask);
    export "DPI-C" function read_register;
    function int read_register(input int idx);
        read_register = (idx == 0) ? 0 : rf.rf[idx[4:0]];
    endfunction
    export "DPI-C" function read_pc;
    function int read_pc();
        read_pc = pc;
    endfunction

    reg reset;
    always @(posedge clk) begin
        reset <= rst;
    end

    reg         valid;
    always @(posedge clk) begin
        if (reset) begin
            valid <= 1'b0;
        end
        else begin
            valid <= 1'b1;
        end
    end

    wire [6:0] opcode = inst[6:0];
    wire [2:0] func3  = inst[14:12];
    wire [6:0] func7  = inst[31:25];
    wire [4:0] rd     = inst[11:7];
    wire [4:0] rs1    = inst[19:15];
    wire [4:0] rs2    = inst[24:20];
    wire [7:0] func3_d;
    wire [127:0] func7_d;
    wire [127:0] opcode_d;
    
    decoder_3_8  u_dec0(.in(func3), .out(func3_d));
    decoder_7_128 u_dec1(.in(func7), .out(func7_d));
    decoder_7_128 u_dec2(.in(opcode), .out(opcode_d));
    wire inst_lui  = opcode_d[7'b0110111];
    wire inst_auipc= opcode_d[7'b0010111];
    wire inst_jal  = opcode_d[7'b1101111];
    wire inst_jalr = opcode_d[7'b1100111] & func3_d[3'b000];
    wire inst_beq  = opcode_d[7'b1100011] & func3_d[3'b000];
    wire inst_bne  = opcode_d[7'b1100011] & func3_d[3'b001];
    wire inst_blt  = opcode_d[7'b1100011] & func3_d[3'b100];
    wire inst_bge  = opcode_d[7'b1100011] & func3_d[3'b101];
    wire inst_bltu = opcode_d[7'b1100011] & func3_d[3'b110];
    wire inst_bgeu = opcode_d[7'b1100011] & func3_d[3'b111];
    wire inst_lb   = opcode_d[7'b0000011] & func3_d[3'b000];
    wire inst_lh   = opcode_d[7'b0000011] & func3_d[3'b001];
    wire inst_lw   = opcode_d[7'b0000011] & func3_d[3'b010];
    wire inst_lbu  = opcode_d[7'b0000011] & func3_d[3'b100];
    wire inst_lhu  = opcode_d[7'b0000011] & func3_d[3'b101];
    wire inst_sb   = opcode_d[7'b0100011] & func3_d[3'b000];
    wire inst_sh   = opcode_d[7'b0100011] & func3_d[3'b001];
    wire inst_sw   = opcode_d[7'b0100011] & func3_d[3'b010];
    wire inst_addi = opcode_d[7'b0010011] & func3_d[3'b000];
    wire inst_slti = opcode_d[7'b0010011] & func3_d[3'b010];
    wire inst_sltiu= opcode_d[7'b0010011] & func3_d[3'b011];
    wire inst_xori = opcode_d[7'b0010011] & func3_d[3'b100];
    wire inst_ori  = opcode_d[7'b0010011] & func3_d[3'b110];
    wire inst_andi = opcode_d[7'b0010011] & func3_d[3'b111];
    wire inst_slli = opcode_d[7'b0010011] & func3_d[3'b001] & func7_d[7'b0000000];
    wire inst_srli = opcode_d[7'b0010011] & func3_d[3'b101] & func7_d[7'b0000000];
    wire inst_srai = opcode_d[7'b0010011] & func3_d[3'b101] & func7_d[7'b0100000];
    wire inst_add  = opcode_d[7'b0110011] & func3_d[3'b000] & func7_d[7'b0000000];
    wire inst_sub  = opcode_d[7'b0110011] & func3_d[3'b000] & func7_d[7'b0100000];
    wire inst_sll  = opcode_d[7'b0110011] & func3_d[3'b001] & func7_d[7'b0000000];
    wire inst_slt  = opcode_d[7'b0110011] & func3_d[3'b010] & func7_d[7'b0000000];
    wire inst_sltu  = opcode_d[7'b0110011] & func3_d[3'b011] & func7_d[7'b0000000];
    wire inst_xor   = opcode_d[7'b0110011] & func3_d[3'b100] & func7_d[7'b0000000];
    wire inst_srl   = opcode_d[7'b0110011] & func3_d[3'b101] & func7_d[7'b0000000];
    wire inst_sra   = opcode_d[7'b0110011] & func3_d[3'b101] & func7_d[7'b0100000];
    wire inst_or    = opcode_d[7'b0110011] & func3_d[3'b110] & func7_d[7'b0000000];
    wire inst_and   = opcode_d[7'b0110011] & func3_d[3'b111] & func7_d[7'b0000000];
    wire inst_ebreak = (inst == 32'h00100073);

    wire need_imm_i;
    wire need_imm_s;
    wire need_imm_u;
    wire need_imm_j;
    wire need_imm_b;
    wire src2_is_4;

    wire [31:0] seq_pc;
    wire [31:0] next_pc;
    wire        br_taken;
    wire [31:0] br_target;
    
    wire [10:0] alu_op;
    wire        load_op;
    wire        src1_is_pc;
    wire        src2_is_imm;
    wire        res_from_mem;
    wire        gr_we;
    wire        mem_we;
    wire [4:0]  dest;
    wire [31:0] rs1_value;
    wire [31:0] rs2_value;
    wire [31:0] imm;
    wire [31:0] br_offs;
    wire [31:0] jal_offs;
    wire        rs1_eq_rs2;
    wire        rs1_lt_rs2;
    wire        rs1_ltu_rs2;

    wire [4:0]  rf_raddr1;
    wire [4:0]  rf_raddr2;
    wire [31:0] rf_rdata1;
    wire [31:0] rf_rdata2;
    wire        rf_we;
    wire [4:0]  rf_waddr;
    wire [31:0] rf_wdata;

    wire [31:0] alu_src1;
    wire [31:0] alu_src2;
    wire [31:0] alu_result;
    wire [31:0] final_result;

    wire [31:0] mem_result;
    wire [31:0] ram_wdata;
    wire [7:0] lb_result;
    wire [15:0] lh_result;
    wire [7:0]   ram_mask;

    assign seq_pc = pc + 4;
    assign next_pc = (br_taken) ? br_target : seq_pc;
    
    always @(posedge clk) begin
        if (reset) begin
            pc <= RESET_PC - 4;
        end
        else if (valid) begin
            pc <= next_pc;
        end
    end

    assign alu_op[0] = inst_addi| inst_auipc | inst_jal 
                        | inst_jalr | inst_add | inst_sb | inst_sh | inst_sw
                        | inst_lb | inst_lh | inst_lw | inst_lbu | inst_lhu;
    assign alu_op[1] = inst_sub;
    assign alu_op[2] = inst_slti | inst_slt;
    assign alu_op[3] = inst_sltiu | inst_sltu;
    assign alu_op[4] = inst_andi | inst_and;
    assign alu_op[5] = inst_ori  | inst_or;
    assign alu_op[6] = inst_xori | inst_xor;
    assign alu_op[7] = inst_slli | inst_sll;
    assign alu_op[8] = inst_srli | inst_srl;
    assign alu_op[9] = inst_srai | inst_sra;
    assign alu_op[10]= inst_lui;

    assign need_imm_i = inst_lb | inst_lh | inst_lw | inst_lbu | inst_lhu 
                        |inst_jalr| inst_addi | inst_slti | inst_sltiu 
                        | inst_xori | inst_ori | inst_andi 
                        | inst_slli | inst_srli | inst_srai;
    assign need_imm_s = inst_sb | inst_sh | inst_sw;
    assign need_imm_u = inst_lui | inst_auipc;
    assign need_imm_j = inst_jal;
    assign need_imm_b = inst_beq | inst_bne | inst_blt | inst_bge | inst_bltu | inst_bgeu;
    assign src2_is_4 = inst_jal| inst_jalr;
    //这个地方只表明指令中如果本身含有有立即数，立即数类型是什么
    assign imm = src2_is_4 ? 32'h4 : 
                 (need_imm_i ? {{20{inst[31]}}, inst[31:20]} :
                 (need_imm_s ? {{20{inst[31]}}, inst[31:25], inst[11:7]} :
                 (need_imm_u ? {inst[31:12], 12'b0} : 32'b0)));
    //这个地方只表明，如果对应的指令需要送入ALU的第二操作数是立即数，那么立即数是什么
    //例如对于jal和jalr指令，虽然它们需要立即数，但送入ALU的第二操作数是4而不是指令中的立即数
    assign br_offs = {{20{inst[31]}}, inst[7], inst[30:25], inst[11:8], 1'b0}; 

    assign jal_offs = inst_jal ? {{12{inst[31]}}, inst[19:12], inst[20], inst[30:21], 1'b0} : 
                      {{20{inst[31]}}, inst[31:20]};

    assign src1_is_pc = inst_auipc | inst_jal | inst_jalr;
    assign src2_is_imm = inst_lb | inst_lh | inst_lw | inst_lbu | inst_lhu 
                        |inst_jalr| inst_addi | inst_slti | inst_sltiu 
                        | inst_xori | inst_ori | inst_andi 
                        | inst_slli | inst_srli | inst_srai 
                        | inst_sb | inst_sh | inst_sw |
                        inst_lui | inst_auipc | inst_jal;
    
    assign res_from_mem = inst_lb | inst_lh | inst_lw | inst_lbu | inst_lhu;
    assign gr_we = ~inst_sb & ~inst_sh & ~inst_sw & ~inst_beq & ~inst_bne & ~inst_blt & ~inst_bge & ~inst_bltu & ~inst_bgeu;
    assign mem_we = inst_sb | inst_sh | inst_sw;
    assign dest   = rd;

    assign rf_raddr1 = rs1;
    assign rf_raddr2 = rs2;
    assign rf_we    = gr_we && valid;
    assign rf_waddr = dest;
    assign rf_wdata = final_result;

    RegisterFile rf(
        .clk(clk),
        .wdata(rf_wdata),
        .waddr(rf_waddr),
        .wen(rf_we),
        .raddr1(rf_raddr1),
        .raddr2(rf_raddr2),
        .rdata1(rf_rdata1),
        .rdata2(rf_rdata2)
    );

    assign rs1_value = rf_rdata1;
    assign rs2_value = rf_rdata2;
    assign rs1_eq_rs2 = (rs1_value == rs2_value);
    assign rs1_lt_rs2 = ($signed(rs1_value) < $signed(rs2_value));
    assign rs1_ltu_rs2 = (rs1_value < rs2_value);

    assign br_taken = ((inst_beq  & rs1_eq_rs2) |
                      (inst_bne  & ~rs1_eq_rs2) |
                      (inst_blt  & rs1_lt_rs2) |
                      (inst_bge  & ~rs1_lt_rs2) |
                      (inst_bltu & rs1_ltu_rs2) |
                      (inst_bgeu & ~rs1_ltu_rs2) |
                      inst_jal |
                      inst_jalr) & valid;  
    assign br_target = (inst_beq | inst_bne | inst_blt | inst_bge | inst_bltu | inst_bgeu) ? (pc + br_offs) :
                       (inst_jalr) ? ((rs1_value + jal_offs) & ~32'h1) : 
                       (inst_jal) ? (pc + jal_offs) : 32'b0;
    
    assign alu_src1 = src1_is_pc ? pc : rs1_value;
    assign alu_src2 = src2_is_imm ? imm : rs2_value;
    alu u_alu(
        .alu_op(alu_op),
        .alu_src1(alu_src1),
        .alu_src2(alu_src2),
        .alu_result(alu_result)
    );

    assign ram_addr = {alu_result[31:2],2'b00}; 
    assign ram_ren  = (inst_lb | inst_lh | inst_lw | inst_lbu | inst_lhu) & valid;
    
    assign lb_result = alu_result[1:0] == 2'b00 ? ram_rdata[7:0] :
                       alu_result[1:0] == 2'b01 ? ram_rdata[15:8] :
                       alu_result[1:0] == 2'b10 ? ram_rdata[23:16] :
                       ram_rdata[31:24];
    assign lh_result = alu_result[1] ? ram_rdata[31:16] : ram_rdata[15:0];
    assign mem_result = (inst_lh) ? {{16{lh_result[15]}}, lh_result} :
                       (inst_lb) ? {{24{lb_result[7]}}, lb_result} :
                       (inst_lhu) ? {16'b0, lh_result} :
                       (inst_lbu) ? {24'b0, lb_result} :
                       ram_rdata;
    assign final_result = (res_from_mem) ? mem_result : alu_result;
   
    assign ram_wdata = (inst_sb) ? {4{rs2_value[7:0]}} :
                       (inst_sh) ? {2{rs2_value[15:0]}} :
                       rs2_value;
    assign ram_mask = (inst_sb) ? (alu_result[1:0] == 2'b00 ? 8'b00000001 :
                                  alu_result[1:0] == 2'b01 ? 8'b00000010 :
                                  alu_result[1:0] == 2'b10 ? 8'b00000100 :
                                                              8'b00001000) :
                       (inst_sh) ? (alu_result[1] ? 8'b00001100 : 8'b00000011) :
                       8'b00001111;
    
    always @(posedge clk) begin
        if (mem_we && valid) begin
            pmem_write(ram_addr, ram_wdata, ram_mask);
        end
    end

     always @(*) begin
        if (inst_ebreak) begin
            ebreak_notify();
        end
    end
endmodule
