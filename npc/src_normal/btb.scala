// ===========================================================================
// BTB (Branch Target Buffer) — 分支目标缓冲
//
// 直接映射, 用 PC 低位索引, 高位做 tag.
// 在 IFU 阶段查询: 命中 → 预测跳转, 提前重定向
// 在 IDU 阶段更新: 分支实际跳转 → 写入 BTB
// ===========================================================================
package npc

import chisel3._
import chisel3.util._

class BTB extends Module {
  val io = IO(new Bundle {
    val lookup_pc  = Input(UInt(32.W))   
    val hit        = Output(Bool())      
    val pred_target = Output(UInt(32.W)) 

    val update_valid  = Input(Bool())    
    val update_pc     = Input(UInt(32.W))
    val update_target = Input(UInt(32.W))
    val update_taken  = Input(Bool())    

    val perf_lookup   = Output(Bool())  
    val perf_hit      = Output(Bool())  
  })

 
  val nEntries  = 4           
  val indexBits = log2Ceil(nEntries)
  val tagBits   = 32 - indexBits - 2 


  val valid = RegInit(VecInit(Seq.fill(nEntries)(false.B)))
  val tags  = RegInit(VecInit(Seq.fill(nEntries)(0.U(tagBits.W))))
  val targets = RegInit(VecInit(Seq.fill(nEntries)(0.U(32.W))))

  val lookup_idx = io.lookup_pc(indexBits + 1, 2)  // 跳过低 2 位字节偏移
  val lookup_tag = io.lookup_pc(31, indexBits + 2)

  io.hit := valid(lookup_idx) && tags(lookup_idx) === lookup_tag
  io.pred_target := targets(lookup_idx)

  io.perf_lookup := true.B
  io.perf_hit    := io.hit

  val update_idx = io.update_pc(indexBits + 1, 2)
  val update_tag = io.update_pc(31, indexBits + 2)

  when(io.update_valid && io.update_taken) {
    valid(update_idx)   := true.B
    tags(update_idx)    := update_tag
    targets(update_idx) := io.update_target
  }
}
