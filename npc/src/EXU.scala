package npc

import chisel3._
import chisel3.util._
class EXU extends Module {
    val io = IO(new Bundle {
        val in = Flipped(Decoupled(new MessageID))
        val out = Decoupled(new MessageEXE)
        
        val awaddr  = Output(UInt(32.W))
        val awvalid = Output(Bool())
        val awready = Input(Bool())
        val awid    = Output(UInt(4.W))
        val awlen   = Output(UInt(8.W))
        val awsize  = Output(UInt(3.W))
        val awburst = Output(UInt(2.W))

        val wdata  = Output(UInt(32.W))
        val wstrb  = Output(UInt(4.W))
        val wvalid = Output(Bool())
        val wready = Input(Bool())
        val wlast  = Output(Bool())

        val araddr  = Output(UInt(32.W))
        val arvalid = Output(Bool())
        val arready = Input(Bool())
        val arid    = Output(UInt(4.W))
        val arlen   = Output(UInt(8.W))
        val arsize  = Output(UInt(3.W))
        val arburst = Output(UInt(2.W))

        // Performance counter events
        val perf_compute     = Output(Bool())  // non-memory ALU done
        val perf_load_issue  = Output(Bool())  // load instruction fired
        val perf_store_issue = Output(Bool())  // store instruction fired
    })
    
    val inst_reg = Reg(UInt(32.W))
    val pc_reg = Reg(UInt(32.W))
    val next_branch_pc_reg = Reg(UInt(32.W))
    val alu_op_reg = Reg(UInt(11.W))
    val alu_src1_reg = Reg(UInt(32.W))
    val alu_src2_reg = Reg(UInt(32.W))
    val write_data_reg = Reg(UInt(32.W))
    val reg_csr_mem_en_dest_reg = Reg(UInt(8.W))
    val mem_en_LS_Type_reg = Reg(UInt(5.W))
    val sys_message_reg = Reg(UInt(4.W))

    val handshake_de = io.in.valid && io.in.ready
    when(handshake_de) {
        inst_reg := io.in.bits.inst
        pc_reg := io.in.bits.pc
        next_branch_pc_reg := io.in.bits.next_branch_pc
        alu_op_reg := io.in.bits.alu_op
        alu_src1_reg := io.in.bits.alu_src1
        alu_src2_reg := io.in.bits.alu_src2
        write_data_reg := io.in.bits.write_data
        reg_csr_mem_en_dest_reg := io.in.bits.reg_csr_mem_en_dest
        mem_en_LS_Type_reg := io.in.bits.mem_en_LS_Type
        sys_message_reg := io.in.bits.sys_message
    }
    
    val handshake_ew = io.out.valid && io.out.ready

    val valid = RegInit(false.B)
    when(handshake_de) {
        valid := true.B
    } .elsewhen(!handshake_de && handshake_ew) {
        valid := false.B
    }

    val alu = Module(new ALU)
    alu.io.alu_op := alu_op_reg
    alu.io.alu_src1 := alu_src1_reg
    alu.io.alu_src2 := alu_src2_reg
    io.out.bits.alu_result := alu.io.alu_result
    //这一级要负责发出访存请求
    val mem_en_reg = mem_en_LS_Type_reg(4)
    val is_load = mem_en_LS_Type_reg(3)
    val func3 = mem_en_LS_Type_reg(2, 0)
    val is_sb = mem_en_LS_Type_reg(4, 3) === "b10".U && func3 === "b000".U
    val is_sh = mem_en_LS_Type_reg(4, 3) === "b10".U && func3 === "b001".U
    val is_sw = mem_en_LS_Type_reg(4, 3) === "b10".U && func3 === "b010".U
    val addr = alu.io.alu_result
    val addr_low = alu.io.alu_result(1, 0)
    val sb_mask = MuxLookup(addr_low, "b1000".U(8.W))(Seq(
        "b00".U -> "b0001".U(8.W),
        "b01".U -> "b0010".U(8.W),
        "b10".U -> "b0100".U(8.W)
    ))
    val sh_mask = Mux(alu.io.alu_result(1), "b1100".U(8.W), "b0011".U(8.W))
    val wmask_val = Mux(is_sb, sb_mask,
                    Mux(is_sh, sh_mask,
                    Mux(is_sw, "b1111".U(8.W), "b0000".U(8.W))))
    val write_data_byte = Fill(4, write_data_reg(7, 0))
    val write_data_half = Fill(2, write_data_reg(15, 0))
    val write_data = Mux(func3 === "b000".U,  write_data_byte,
                    Mux(func3 === "b001".U,  write_data_half,
                    Mux(func3 === "b010".U, write_data_reg, 0.U)))
    val sReadIdle :: sReadReq :: sReadWait :: Nil = Enum(3)
    val sWriteIdle :: sWriteReq :: sWriteData :: sWriteAddr :: sWriteWait :: Nil = Enum(5)
    val state_read = RegInit(sReadIdle)
    val state_write = RegInit(sWriteIdle)
    val new_is_mem = io.in.bits.mem_en_LS_Type(4)
    val new_is_write = new_is_mem && !io.in.bits.mem_en_LS_Type(3)
    val new_is_read = new_is_mem && io.in.bits.mem_en_LS_Type(3)
    switch (state_read) {
        is (sReadIdle) {
            when (handshake_de && new_is_read) {
                state_read := sReadReq
            }
        }
        is (sReadReq) {
            when (io.arvalid && io.arready) {
                when (handshake_de && new_is_read) {
                    state_read := sReadReq
                } .elsewhen(handshake_ew) {
                    state_read := sReadIdle
                } .otherwise {
                    state_read := sReadWait
                }
            }
        }
        is (sReadWait) {
            when (handshake_de && new_is_read) {
                    state_read := sReadReq
            } .elsewhen(handshake_ew) {
                state_read := sReadIdle
            } .otherwise {
                state_read := sReadWait
            }
        }
    }
    io.araddr := addr
    io.arvalid := (state_read === sReadReq) && valid
    io.arid := 1.U
    io.arlen := 0.U
    val is_lb = func3 === "b000".U
    val is_lh = func3 === "b001".U
    val is_lw = func3 === "b010".U
    val is_lbu = func3 === "b100".U
    val is_lhu = func3 === "b101".U
    io.arsize := Mux(is_lb || is_lbu, 0.U, 
                    Mux(is_lh || is_lhu, 1.U, 
                        Mux(is_lw, 2.U, 0.U)))
    io.arburst := 1.U // INCR
    
    val aw_handshake = io.awvalid && io.awready
    val w_handshake = io.wvalid && io.wready
    switch (state_write) {
        is (sWriteIdle) {
            when (handshake_de && new_is_write) {
                state_write := sWriteReq
            }
        }
        is (sWriteReq) {
            when (aw_handshake && w_handshake) {
                when (handshake_de && new_is_write) {
                    state_write := sWriteReq
                } .elsewhen(handshake_ew) {
                    state_write := sWriteIdle
                } .otherwise {
                    state_write := sWriteWait
                }
            } .elsewhen(aw_handshake) {
                state_write := sWriteData
            } .elsewhen(w_handshake) {
                state_write := sWriteAddr
            }
        }
        is (sWriteData) {
            when (w_handshake) {
                when (handshake_de && new_is_write) {
                    state_write := sWriteReq
                } .elsewhen(handshake_ew) {
                    state_write := sWriteIdle
                } .otherwise {
                    state_write := sWriteWait
                }
            }
        }
        is (sWriteAddr) {            
            when (aw_handshake) {
                when (handshake_de && new_is_write) {
                    state_write := sWriteReq
                } .elsewhen(handshake_ew) {
                    state_write := sWriteIdle
                } .otherwise {
                    state_write := sWriteWait
                }
            }
        }
        is (sWriteWait) {
            when (handshake_de && new_is_write) {
                    state_write := sWriteReq
            } .elsewhen(handshake_ew) {
                state_write := sWriteIdle
            } .otherwise {
                state_write := sWriteWait
            }
        }
    }
    io.awaddr := addr
    io.awvalid := (state_write === sWriteReq) || (state_write === sWriteAddr)
    io.awid := 1.U
    io.awlen := 0.U
    io.awsize := Mux(is_sb, 0.U,
                    Mux(is_sh, 1.U,
                        Mux(is_sw, 2.U, 0.U)))
    io.awburst := 1.U // INCR
    io.wdata := write_data
    io.wstrb := wmask_val
    io.wvalid := (state_write === sWriteReq) || (state_write === sWriteData)
    io.wlast := state_write === sWriteData || state_write === sWriteAddr || (state_write === sWriteReq )
    io.arvalid := (state_read === sReadReq)
    
    io.out.valid := valid && (!mem_en_reg || 
    (is_load && ((state_read === sReadWait)||(state_read === sReadReq && io.arready))
    )|| (!is_load && ((state_write === sWriteWait)||(state_write === sWriteReq && aw_handshake && w_handshake)
    ||(state_write === sWriteData && w_handshake)|| (state_write === sWriteAddr && aw_handshake))))
    // 所有外设地址空间 (NEMU 不建模的外设, difftest 需要跳过对比)
    val device_addr_in =
      (addr >= "h02000000".U && addr <= "h0200ffff".U) ||  // CLINT
      (addr >= "h10000000".U && addr <= "h10000fff".U) ||  // UART
      (addr >= "h10001000".U && addr <= "h10001fff".U) ||  // SPI 控制器
      (addr >= "h10002000".U && addr <= "h1000200f".U) ||  // GPIO
      (addr >= "h10011000".U && addr <= "h10011007".U) ||  // Keyboard (PS/2)
      (addr >= "h21000000".U && addr <= "h211fffff".U)   // VGA
    val is_device_access = valid && (mem_en_reg) && (device_addr_in) 
    io.out.bits.device_access := is_device_access
    io.out.bits.inst := inst_reg
    io.out.bits.pc := pc_reg
    io.out.bits.next_branch_pc := next_branch_pc_reg
    io.out.bits.alu_result := alu.io.alu_result
    io.out.bits.write_data_csr := write_data_reg
    io.out.bits.mem_en_LS_Type := mem_en_LS_Type_reg
    io.out.bits.reg_csr_mem_en_dest := reg_csr_mem_en_dest_reg
    io.out.bits.sys_message := sys_message_reg
    io.in.ready := !valid || (io.out.valid && io.out.ready)

    // ── Performance counter events ──
    val exu_fire = io.out.valid && io.out.ready
    io.perf_compute     := exu_fire && !mem_en_LS_Type_reg(4)
    io.perf_load_issue  := exu_fire &&  mem_en_LS_Type_reg(4) &&  mem_en_LS_Type_reg(3)
    io.perf_store_issue := exu_fire &&  mem_en_LS_Type_reg(4) && !mem_en_LS_Type_reg(3)
}   