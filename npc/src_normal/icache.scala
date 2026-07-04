package npc

import chisel3._
import chisel3.util._

class ICache(
  val nWays: Int     = 1,
  val nSets: Int     = 4,
  val blockSize: Int = 16  //字节数
) extends Module {

  val offsetBits = log2Ceil(blockSize)
  val indexBits  = log2Ceil(nSets)
  val tagBits    = 32 - offsetBits - indexBits
  val wordCnt    = blockSize / 4
  val wayBits    = log2Ceil(nWays)

  private def tagField(addr:    UInt): UInt = addr(31, offsetBits + indexBits)
  private def indexField(addr:  UInt): UInt = addr(offsetBits + indexBits - 1, offsetBits)
  private def offsetField(addr: UInt): UInt = addr(offsetBits - 1, 2)
  private def blockAlign(addr:  UInt): UInt = Cat(tagField(addr), indexField(addr), 0.U(offsetBits.W))

  val io = IO(new Bundle {
    val if_req = Flipped(new if_sram)
    val axi    = new AXI4Bundle
    val fencei_req = Input(Bool())
  })

  class cache_line extends Bundle {
    val tag  = UInt(tagBits.W)
    val data = UInt((blockSize * 8).W)
  }
  val cache       = Seq.fill(nWays)(SyncReadMem(nSets, new cache_line))
  val cache_valid = RegInit(VecInit(Seq.fill(nWays)(0.U(nSets.W))))
  val cache_fence = RegInit(false.B)
  val fence_deal  = Wire(Bool())
  when (fence_deal) {
    cache_fence := false.B
  } .elsewhen (io.fencei_req) {
    cache_fence := true.B
  }

  val repl_ptr   = RegInit(0.U(wayBits.W))
  val refill_way = RegInit(0.U(wayBits.W))

  val sIDLE :: sLookup :: sReplace :: sRefill :: Nil = Enum(4)
  val state      = RegInit(sIDLE)
  val next_state = Wire(UInt(2.W))
  val lookup     = Wire(Bool())
  val cache_hit     = Wire(Bool())
  // ── Request buffer ──
  val reg_index  = RegInit(0.U(indexBits.W))
  val reg_tag    = RegInit(0.U(tagBits.W))
  val reg_offset = RegInit(0.U(offsetBits.W))
  when (lookup) {
    reg_index  := indexField(io.if_req.addr)
    reg_tag    := tagField(io.if_req.addr)
    reg_offset := offsetField(io.if_req.addr)
  }

  // ── State transitions ──
  when (state === sIDLE) {
    next_state := Mux(cache_fence, sIDLE,
                  Mux(io.if_req.req_valid, sLookup, sIDLE))
  } .elsewhen (state === sLookup) {
    next_state := Mux(!cache_hit, sReplace,
                  Mux(cache_fence, sIDLE,
                  Mux(io.if_req.req_valid, sLookup, sIDLE)))
  } .elsewhen (state === sReplace) {
    next_state := Mux(io.axi.arvalid && io.axi.arready, sRefill, sReplace)
  } .elsewhen (state === sRefill) {
    next_state := Mux(io.axi.rvalid && io.axi.rlast, sIDLE, sRefill)
  } .otherwise {
    next_state := sIDLE
  }
  state  := next_state
  lookup := (state === sIDLE && io.if_req.req_valid && !cache_fence) ||
             (state === sLookup && io.if_req.req_valid && cache_hit && !cache_fence)

  val cache_line_read = Wire(Vec(nWays, new cache_line))
  for (w <- 0 until nWays) {
    cache_line_read(w) := cache(w).read(indexField(io.if_req.addr), lookup)
  }

  val cache_valid_read = Wire(Vec(nWays, Bool()))
  for (w <- 0 until nWays) {
    cache_valid_read(w) := cache_valid(w)(reg_index)
  }

  val cache_hit_ways = Wire(Vec(nWays, Bool()))
  for (w <- 0 until nWays) {
    cache_hit_ways(w) := (cache_line_read(w).tag === reg_tag) && cache_valid_read(w)&& (state === sLookup)
  }
  val cache_hit_way = PriorityEncoder(cache_hit_ways)
  cache_hit := cache_hit_ways.reduce(_ || _)

  // ── Round-robin: select replace way on miss ──
  when (state === sLookup) {
    refill_way := repl_ptr
    repl_ptr := repl_ptr + 1.U
  }


  io.axi.araddr  := Cat(reg_tag, reg_index, 0.U(offsetBits.W))
  io.axi.arvalid := state === sReplace
  io.axi.arid    := 0.U
  io.axi.arlen   := (wordCnt - 1).U
  io.axi.arsize  := log2Ceil(4).U
  io.axi.arburst := 1.U


  io.axi.awaddr  := 0.U; io.axi.awvalid := false.B
  io.axi.awid    := 0.U; io.axi.awlen   := 0.U
  io.axi.awsize  := 0.U; io.axi.awburst := 0.U
  io.axi.wdata   := 0.U; io.axi.wstrb   := 0.U
  io.axi.wvalid  := false.B; io.axi.wlast := false.B
  io.axi.bready  := false.B

  io.axi.rready := state === sRefill

  val rdata_buffer = Reg(UInt((blockSize * 8).W))
  val rdata_cnt    = RegInit(0.U(log2Ceil(wordCnt).W))

  // 组合拼接: 旧buffer + 当前拍rdata → 完整数据块 (解决rlast时寄存器未更新问题)
  val refill_buf_comb = Cat(io.axi.rdata, rdata_buffer((blockSize * 8 - 1), 32))

  when (state === sRefill && io.axi.rvalid && io.axi.rready) {
    rdata_buffer := refill_buf_comb
    rdata_cnt    := rdata_cnt + 1.U
  }
  when (state =/= sRefill) {
    rdata_cnt    := 0.U
    rdata_buffer := 0.U
  }

  when (state === sRefill && io.axi.rvalid && io.axi.rready && io.axi.rlast) {
    for (w <- 0 until nWays) {
      when (refill_way === w.U) {
        cache(w).write(reg_index, Cat(reg_tag, refill_buf_comb).asTypeOf(new cache_line))
      }
    }
  }

  when (state === sRefill && io.axi.rvalid && io.axi.rready && io.axi.rlast) {
    for (w <- 0 until nWays) {
      when (refill_way === w.U) {
          cache_valid(w) := cache_valid(w) | UIntToOH(reg_index)
      }
    }
  } .elsewhen (state === sIDLE && cache_fence) {
    for (w <- 0 until nWays) {
      cache_valid(w) := 0.U
    }
  }
  fence_deal := state === sIDLE && cache_fence

  val hit_word    = (cache_line_read(cache_hit_way).data >> (reg_offset * 32.U))(31, 0)
  val refill_word = (refill_buf_comb >> (reg_offset * 32.U))(31, 0)
  val refill_done = state === sRefill && io.axi.rvalid && io.axi.rready && io.axi.rlast

  // addr_ok: 接受新地址的握手信号
  io.if_req.addr_ok := lookup
  // data_ok: 数据有效的握手信号
  io.if_req.data_ok := (state === sLookup && cache_hit) || refill_done
  io.if_req.rdata := Mux(refill_done, refill_word, hit_word)
}
