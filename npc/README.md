
该目录如下:
src:scala代码
src_normal:scala代码去掉dpi相关部分，用于时序分析
branchsim:分支模拟器
cachesim:dcache模拟器
icache_sim:icache模拟器
soc_csrc:soc的csrc文件
soc_minimal:最小化的仿真文件，用于加速仿真
通过在编译时选择使用哪一套可以用NODPI=1 或 0

Chisel Project Template
=======================

Another version of the [Chisel template](https://github.com/ucb-bar/chisel-template) supporting mill.
mill is another Scala/Java build tool without obscure DSL like SBT. It is much faster than SBT.

Contents at a glance:

* `.gitignore` - helps Git ignore junk like generated files, build products, and temporary files.
* `build.mill` - instructs mill to build the Chisel project
* `Makefile` - rules to call mill
* `src/GCD.scala` - GCD source file
* `src/DecoupledGCD.scala` - another GCD source file
* `src/Elaborate.scala` - wrapper file to call chisel command with the GCD module
* `test/src/GCDSpec.scala` - GCD tester

Feel free to rename or delete files under `src/` and `test/` or use them as a reference/template.

## Getting Started

First, install mill by referring to the documentation [here](https://com-lihaoyi.github.io/mill).

To run all tests in this design (recommended for test-driven development):
```bash
make test
```

To generate Verilog:
```bash
make verilog
```

