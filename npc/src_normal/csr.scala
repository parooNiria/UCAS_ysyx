package npc

import chisel3._
import chisel3.util._
// ======================================
// CSR
// ======================================
class CSR extends Module {
  val io = IO(new Bundle {
    //csr读写信息
    val csr_we    = Input(Bool())
    val csr_waddr = Input(UInt(12.W))
    val csr_wmask = Input(UInt(32.W))
    val csr_wdata = Input(UInt(32.W))
    val csr_raddr = Input(UInt(12.W))
    val csr_rdata = Output(UInt(32.W))
    //特权相关信息
    val ex        = Input(Bool())
    val epc       = Input(UInt(32.W))
    val cause     = Input(UInt(32.W))
    val mret      = Input(Bool())
    
    val mtvec_val = Output(UInt(32.W))
    val mepc_val  = Output(UInt(32.W))
    val mstatus_val = Output(UInt(32.W))
    val mcause_val = Output(UInt(32.W))
  })
  object CSRAddr {
    val MVENDORID = "hF11".U(12.W)
    val MARCHID   = "hF12".U(12.W)
    val MSTATUS   = "h300".U(12.W)
    val MTVEC     = "h305".U(12.W)
    val MEPC      = "h341".U(12.W)
    val MCAUSE    = "h342".U(12.W)
    val MCYCLE    = "hB00".U(12.W)
    val MCYCLEH   = "hB80".U(12.W)
  }


  object CSR_MSTATUS {
    val MIE  = 3
    val MPIE = 7
    val MPP  = 11  // 对应 Verilog 的 12:11
  }
  //周期计数器
  val mcycle = RegInit(0.U(64.W))
  mcycle := mcycle + 1.U
  //ysyx寄存器
  val mvendorid = RegInit("h79737978".U(32.W))
  val marchid   = RegInit("h18da591".U(32.W))

  
  val mstatus   = RegInit("h1800".U(32.W))//M-mode
  when(io.mret) {
      val next_mie = mstatus(CSR_MSTATUS.MPIE)
      
      mstatus := (mstatus & ~( (1.U << CSR_MSTATUS.MIE) | (1.U << CSR_MSTATUS.MPIE) | (3.U << CSR_MSTATUS.MPP) )) | (next_mie << CSR_MSTATUS.MIE) | (1.U << CSR_MSTATUS.MPIE) | (0.U << CSR_MSTATUS.MPP)
  } .elsewhen (io.ex) {
    val next_mpie = mstatus(CSR_MSTATUS.MIE)
    
    mstatus := (mstatus & ~( (1.U << CSR_MSTATUS.MIE) | (1.U << CSR_MSTATUS.MPIE) | (3.U << CSR_MSTATUS.MPP) )) | (0.U << CSR_MSTATUS.MIE) | (next_mpie << CSR_MSTATUS.MPIE) | (3.U << CSR_MSTATUS.MPP)
  } .elsewhen (io.csr_we && io.csr_waddr === CSRAddr.MSTATUS) {
    mstatus := (mstatus & ~io.csr_wmask) | (io.csr_wdata & io.csr_wmask)
  }

  val mtvec     = RegInit(0.U(32.W))
  val mepc      = RegInit(0.U(32.W))
  val mcause    = RegInit(0.U(32.W))
  when(io.ex) {
    mepc := io.epc
    mcause := io.cause
  }.elsewhen(io.csr_we) {
    when(io.csr_waddr === CSRAddr.MTVEC) {
      mtvec := (mtvec & ~io.csr_wmask) | (io.csr_wdata & io.csr_wmask)
    } .elsewhen(io.csr_waddr === CSRAddr.MEPC) {
      mepc := (mepc & ~io.csr_wmask) | (io.csr_wdata & io.csr_wmask)
    } .elsewhen(io.csr_waddr === CSRAddr.MCAUSE) {
      mcause := (mcause & ~io.csr_wmask) | (io.csr_wdata & io.csr_wmask)
    }
  }

  io.csr_rdata := MuxLookup(io.csr_raddr, 0.U)(Seq(
    "hF11".U -> mvendorid,
    "hF12".U -> marchid,
    "h300".U -> mstatus,
    "h305".U -> mtvec,
    "h341".U -> mepc,
    "h342".U -> mcause,
    "hB00".U -> mcycle(31, 0),
    "hB80".U -> mcycle(63, 32)
  ))

  io.mtvec_val := mtvec
  io.mepc_val  := mepc
  io.mstatus_val := mstatus
  io.mcause_val := mcause
}