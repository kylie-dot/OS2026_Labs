# Lab5 提示词汇总


## 前置实验迁移：Trap 汇编入口、返回与时钟中断

**负责人：** 2410690-王睿瑜

### 一、Spec（设计依据）

**1. 输入与调用条件**

- CPU 通过 `stvec` 进入 `__alltraps`。保存区应与 `struct trapframe` 的布局一致，并按 `sscratch` 约定处理不同来源的栈。
- `trap_dispatch()` 根据 `scause` 的最高位区分中断与异常。时钟中断的原因码取去掉最高位后的值。

**2. 输出与处理过程**

- 向 C 层传递完整 Trap 帧，并根据保存的 `SPP` 为返回 U 态或 S 态设置 `sscratch`。恢复通用寄存器、`sstatus` 和 `sepc` 后执行 `sret`。
- 时钟中断重设下一次事件，增加 `ticks`，并在存在当前进程时设置 `need_resched`。

**3. 修改范围与验收**

- 修改 `code/kern/trap/trapentry.S` 的入口、返回代码和 `code/kern/trap/trap.c` 的中断处理区域。
- 分别重编译 `trapentry.o` 和 `trap.o`，运行时核对 U 态系统调用返回与 S 态中断恢复。

### 二、最终提示词

```text
[PROMPT]
在 RISC-V uCore Lab5 中补全 code/kern/trap/trapentry.S 的 Lab3 迁移区域。
实现 __alltraps 与 RESTORE_ALL，使内核可以处理来自 U 态和 S 态的 Trap。

[RELY]
阅读 trapentry.S 现有的 SAVE_ALL、trap.h 的 trapframe、riscv.h 的
REGBYTES 与 SSTATUS_SPP。检查 forkrets 和 __trapret 的衔接。

[GUARANTEE]
只编辑 trapentry.S 的指定区域，保持 trapframe 布局和已有汇编符号。
用 make -C code -B V= obj/kern/trap/trapentry.o 验证，保留真实输出。

[SPECIFICATION]
1. 进入 __alltraps 后保存现场，将当前 sp 作为 trapframe 地址传给 trap。
2. 根据保存的 sstatus.SPP 设置 sscratch，使返回 U 态后可切回内核栈，
   并使来自 S 态的 Trap 继续按内核态约定处理。
3. 恢复 sstatus、sepc、通用寄存器和原 sp，由 sret 完成返回。
```

### 三、实现迭代过程

| 阶段 | 实际现象 | 处理与结果 |
| --- | --- | --- |
| 首次编译检查 | `trapentry.o` 和 `trap.o` 显示 `is up to date`。 | AI 帮助判读日志，确认本次复用了已有文件。 |
| 强制复编译 | 使用 `make -C code -B V= ...`，日志显示实际调用 RISC-V GCC。 | 两个目标均重新编译通过。特权级转换的运行证据待双 GDB 补录。 |


## 功能模块一：系统调用入口与分发

**负责人：** 2410690-王睿瑜

### 一、Spec（设计依据）

**1. 输入与调用条件**

- `exception_handler()` 接收 Trap 帧。现有 `trap()` 在分发前把 `current->tf` 指向本次现场。
- 用户库使用 `a0` 传系统调用号，`a1`～`a5` 传参数。缺页接口按小组契约接收 `current->mm`、RISC-V 异常号和故障地址。

**2. 输出与处理过程**

- 用户态 `ecall` 将 `epc` 推进 4 字节，再进入 `syscall()`。编号越界或表项为空时，`a0` 返回 `-E_INVAL`。有效编号通过包装函数分发，结果也写回 `a0`。
- 可处理的缺页保留 `epc`，供返回后重试。无法处理的异常打印现场，S 态触发 `panic()`，正常有当前进程的 U 态路径调用 `do_exit(-E_KILLED)`。

**3. 修改范围与验收**

- 修改 `trap.c` 和 `syscall.c` 的模块一标记区域。保留公开签名和系统调用编号。
- 重编译 `trap.o`、`syscall.o`。对照 `test_lab5_syscall()` 的条件检查 `epc` 推进、`SYS_getpid` 的返回值和调用号 0 的处理。

### 二、最终提示词

```text
[PROMPT]
补全 code/kern/trap/trap.c 里的 Lab3 时钟中断处理和 Lab5 function1
异常分发。以现有 trap() 为入口检查实现。

[RELY]
读取 trap.h、riscv.h、clock.h、syscall.h、proc.h 和小组缺页契约。
do_pgfault 接收 current->mm、RISC-V 异常号及故障地址。
trap() 已负责安装本次 current->tf 和处理返回用户态前的调度。

[GUARANTEE]
保留已有 trap() 主流程和公开接口，保留 function1 标记并填写学号 2410690。
执行 make -C code -B V= obj/kern/trap/trap.o，记录实际编译结果。

[SPECIFICATION]
1. S 态时钟中断重设下一次时钟事件并累加 ticks。有 current 时设置
   need_resched。依据 scause 的最高位区分中断标志与原因编号。
2. CAUSE_USER_ECALL 先推进 epc 4 字节，再调用 syscall()。
3. 取指、读和写缺页在 current->mm 存在时调用 do_pgfault。
   成功则保留 epc 并返回。失败时打印现场，内核态异常 panic，
   用户态异常用 do_exit(-E_KILLED) 退出当前进程。
```

```text
[PROMPT]
只编辑 code/kern/syscall/syscall.c，完成 function1 的 syscall()
和现有分发表对应的九个 sys_* 包装函数。

[RELY]
先读取 code/user/libs/syscall.c、proc.h、unistd.h 和 tools/test_lab5.c，
沿用现有参数类型和接口。按小组约定，sys_pgdir 返回 0。

[GUARANTEE]
保留现有系统调用编号和公开接口，function1 注释填写学号 2410690。
执行 git diff --check 和 make -C code -B V= obj/kern/syscall/syscall.o。

[SPECIFICATION]
1. 从 current->tf 的 a0 读编号，从 a1～a5 依次取参数。
2. 访问分发表前检查编号范围，再检查表项是否为空。
   非法或未实现的编号在 a0 返回 -E_INVAL。
3. 有效调用通过包装函数进入相应内核逻辑，结果写回 a0。
   对照 test_lab5_syscall 检查 SYS_getpid 和调用号 0 的预期行为。
```

### 三、实现迭代过程

| 阶段 | 实际现象 | 处理与结果 |
| --- | --- | --- |
| 接口复核 | 审查时需要确认 `do_pgfault()` 的第二个参数。 | 对照小组契约确认直接传递 RISC-V 异常号，保留现有调用。 |
| 源码与编译核对 | `syscall()` 已有函数体，模块标记仍显示 `YOUR CODE`。VS Code 对若干符号画出红线。 | 按要求核对学号标记，以 WSL 的交叉编译结果判断这些符号是否有编译问题。`syscall.o` 强制重编译通过。 |


## 功能模块四：其余系统调用功能

**负责人：** 2410690-王睿瑜

### 一、Spec（设计依据）

**1. 输入与调用条件**

- `do_exit(error_code)` 面向当前进程，使用 PCB、`mm`、父子链及等待状态。`do_yield()` 面向当前进程。`do_kill(pid)` 通过 PID 查找目标。
- 退出、等待最终回收及地址空间释放各自使用仓库已有接口，并遵守 `do_wait()` 对僵尸进程的回收约定。

**2. 输出与处理过程**

- 退出时先脱离用户地址空间，再处理引用计数。记录退出码，进入僵尸态，唤醒等待的父进程并移交子进程。当前内核栈和 PCB 由等待方回收。
- 主动让出返回 0 并设置 `need_resched`。终止请求在中断保护下设置 `PF_EXITING`，可中断睡眠的目标由 `wakeup_proc()` 唤醒。

**3. 修改范围与验收**

- 只补 `code/kern/process/proc.c` 的模块四，保留模块二和模块三区域。
- 单编译 `proc.o`，对照 `test_lab5_process_control()` 的相关断言，集成后再用 `exit` 和 `spin` 检查完整生命周期。

### 二、最终提示词

```text
[PROMPT]
实现 code/kern/process/proc.c 的 function4，即 do_exit、do_yield、do_kill。

[RELY]
阅读 proc.c、proc.h、vmm.h、riscv.h、sched.c、trap.c 和 tools/test_lab5.c。
沿用现有页表切换、mm 引用计数、中断保护和进程树接口。
核对与 do_wait 的回收分工。

[GUARANTEE]
仅修改 proc.c 的 function4 区域，保留公开签名和其他模块，填写学号 2410690。
执行 git diff --check 和 make -C code -B V= obj/kern/process/proc.o。

[SPECIFICATION]
1. do_exit 保护 idleproc 和 initproc。若有当前用户地址空间，先切回
   启动页表并刷新 TLB，再减少 mm 引用计数，最后一个引用才销毁地址空间。
   写入退出码并设置 PROC_ZOMBIE，唤醒正在等待子进程退出的父进程。
   将子进程移交给 initproc，维护双向兄弟链接。有僵尸子进程且 initproc
   正在等待时将其唤醒。用中断保护更新相关状态，最后 schedule()。
   进程结构与内核栈由 do_wait 回收。
2. do_yield 设置 current->need_resched 并返回 0。
3. do_kill 查找目标 PID，找不到返回 -E_INVAL，重复退出请求返回 -E_KILLED。
   第一次有效请求设置 PF_EXITING，仅唤醒处于可中断睡眠的目标。
4. 对照 test_lab5_process_control 检查 do_yield 和 do_kill(-1)。
```

### 三、实现迭代过程

| 阶段 | 实际现象 | 处理与结果 |
| --- | --- | --- |
| 确定实现位置 | WSL 未安装 `rg`，改用 `grep` 查询，确认 `function4` 标记与后续宏之间为空。 | 用四段式提示词限定 AI 只补模块四。 |
| 审查和复编译 | 新代码包含 `do_exit()`、`do_yield()`、`do_kill()`，并标记学号 2410690。 | 执行 `git diff --check` 并强制编译 `proc.o`，两项通过。进程回收与终止后的等待仍待整机验证。 |


## 分支任务：双 GDB 跟踪系统调用

**负责人：** 2410690-王睿瑜

### 一、Spec（设计依据）

**1. 输入与调用条件**

- 已合并并能运行用户程序的 Lab5 代码、实际装载的用户 ELF，以及带调试符号的 QEMU 4.1.1。

**2. 输出与处理过程**

- 对同一个能返回的用户态系统调用，分别记录 guest 的 CSR、寄存器和指令，以及宿主侧的 TCG 翻译、运行期陷入与返回路径。

**3. 验收**

- 用源文件位置和两侧断点记录连接 `ecall`、内核分发和 `sret`。

### 二、最终提示词

```text
[PROMPT]
协助我在可运行的 Lab5 集成版本和带调试符号的 QEMU 4.1.1 上，
实测一个会返回的用户态系统调用从 ecall 到 sret 的完整过程。

[RELY]
先检查当前 Makefile 的 debug 和 gdb 目标、实际装载的用户 ELF，
以及本地 QEMU 4.1.1 源码的 RISC-V 译码与运行期处理函数。
宿主 GDB 用于观察 QEMU，guest GDB 用于观察 uCore。

[GUARANTEE]
先让 guest GDB 停在实际执行的 ecall，再设置宿主侧断点。
报告仅记录本次实测的源码位置、地址与寄存器值。
若断点未触发，记录执行命令、实际现象和排查结果。

[SPECIFICATION]
1. 记录 ecall 前的用户 PC、a0，陷入后的 scause、sepc、sstatus。
2. 跟踪异常分发和系统调用对 trapframe 内 epc、a0 的修改。
3. 记录 sret 前的 sepc 和 SPP，以及返回后的 PC、a0、特权级。
4. 核对两条指令从 TCG 翻译到运行期处理的路径，记录对应源码位置。
```

### 三、实现迭代过程

待在可运行的集成版本上执行，实测后补录断点、输出和实际排查过程。
