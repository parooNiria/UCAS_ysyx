package npc

import chisel3._
import chisel3.util._

class MessageIF extends Bundle {
    val inst = Output(UInt(32.W))
    val pc = Output(UInt(32.W))
}

class MessageID extends Bundle {
    val inst = Output(UInt(32.W))
    val pc = Output(UInt(32.W))
    val next_branch_pc = Output(UInt(32.W))
    val alu_op = Output(UInt(11.W))
    val alu_src1 = Output(UInt(32.W))
    val alu_src2 = Output(UInt(32.W))
    val write_data = Output(UInt(32.W))
    val reg_csr_mem_en_dest = Output(UInt(8.W))
    val mem_en_LS_Type = Output(UInt(5.W))
    val sys_message = Output(UInt(3.W))
}

class MessageEXE extends Bundle {
    val inst = Output(UInt(32.W))
    val pc = Output(UInt(32.W))
    val next_branch_pc = Output(UInt(32.W))
    val alu_result = Output(UInt(32.W))
    val write_data_csr = Output(UInt(32.W))
    val mem_en_LS_Type = Output(UInt(5.W))
    val reg_csr_mem_en_dest = Output(UInt(8.W))
    val sys_message = Output(UInt(3.W))
    val device_access = Output(Bool())
}

class MessageMEM extends Bundle {
    val inst = Output(UInt(32.W))
    val pc = Output(UInt(32.W))
    val next_branch_pc = Output(UInt(32.W))
    val reg_write_data = Output(UInt(32.W))
    val csr_write_data = Output(UInt(32.W))
    val reg_csr_en_dest = Output(UInt(7.W))
    val sys_message = Output(UInt(3.W))
    val device_access = Output(Bool())
}

class rf_read extends Bundle {
    val raddr1 = Output(UInt(5.W))
    val raddr2 = Output(UInt(5.W))
    val rdata1 = Input(UInt(32.W))
    val rdata2 = Input(UInt(32.W))
    //留着将来冲突扩展
}


class CommitUpdate extends Bundle {
    val next_pc = Output(UInt(32.W))
    val commit_valid = Output(Bool())
}

class CommitInfo extends Bundle {
    val inst = Output(UInt(32.W))
    val pc = Output(UInt(32.W))
    val next_pc = Output(UInt(32.W))
    val reg_dest = Output(UInt(5.W))
    val reg_write_data = Output(UInt(32.W))
    val reg_we_en = Output(Bool())
    val commit_valid = Output(Bool())
    val device_access = Output(Bool())
}

class AXI4Lite extends Bundle {
    val awaddr  = Output(UInt(32.W))
    val awvalid = Output(Bool())
    val awready = Input(Bool())

    val wdata  = Output(UInt(32.W))
    val wstrb  = Output(UInt(4.W))
    val wvalid = Output(Bool())
    val wready = Input(Bool())

    val bresp  = Input(UInt(2.W))
    val bvalid = Input(Bool())
    val bready = Output(Bool())

    val araddr  = Output(UInt(32.W))
    val arvalid = Output(Bool())
    val arready = Input(Bool())

    val rdata  = Input(UInt(32.W))
    val rresp  = Input(UInt(2.W))
    val rvalid = Input(Bool())
    val rready = Output(Bool())
}