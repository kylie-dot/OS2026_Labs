# 操作系统实验报告

## 实验基本信息

| 项目 | 内容 |
|------|------|
| **实验名称** | Lab X: [实验名称] |
| **小组成员** | 学号1-姓名1、学号2-姓名2、学号3-姓名3 |
| **完成日期** | YYYY-MM-DD |

### 小组分工

练习如何分工？

| 成员 | 负责的练习/模块 |
|------|----------------|
| 学号1-姓名1 | [练习或模块名称] |
| 学号2-姓名2 | [练习或模块名称] |
| 学号3-姓名3 | [练习或模块名称] |

实验报告如何分工？

---

## 一、实验目的

<!-- 说明本次实验的主要目标和预期成果 -->

本实验的主要目的是：

1. [目的1：例如，理解用户态进程如何通过系统调用进入内核]
2. [目的2：例如，掌握进程创建、执行、退出的完整生命周期]
3. [目的3：例如，学习使用 AI 辅助开发操作系统核心功能]

---

## 二、实验环境

你们使用的 AI 工具

| 成员 | AI 编程工具 | 底层模型 | 备注 |
|------|------------|---------|------|
| 学号1-姓名1 | [例如：Cline（VS Code插件） / Claude Code（终端 Agent） / DeepSeek Harness等] | [例如：Claude Sonnet 4.5 / GPT-5.5 / DeepSeek-V4-Flash等] | [可选：特殊配置等] |
| 学号2-姓名2 | | | |
| 学号3-姓名3 | | | |

**说明：**
- **AI 编程工具**：指具体使用的终端工具、编辑器插件、桌面应用或浏览器界面
- **底层模型**：指该工具使用的大语言模型及版本

---

## 三、实验整体逻辑分析

### 2.1 本章节的逻辑主线

[请用自己的话描述本章实验的核心主线，例如：
- 本章围绕哪个核心主题展开？（例如：用户进程管理、虚拟内存、文件系统等）
- 整个实验解决什么问题？（例如：如何让用户程序运行在操作系统之上）]

### 2.2 功能的逐步实现

[说明本章实验是按照什么顺序逐步实现各个功能的，以及为什么要按这个顺序，例如：

1. 首先实现**功能A** → 为什么首先实现这个？（例如：建立系统调用接口是后续所有用户态功能的基础）
2. 接着再完善**功能B** → 为什么接着实现这个？（例如：有了系统调用后，需要能够加载和启动第一个用户进程）
3. 然后完成**功能C** → 为什么然后实现这个？（例如：有了第一个进程后，需要实现进程复制功能）
4. 最后补充了**功能D** → 为什么最后实现这个？（例如：最后完善进程退出和清理机制，形成完整闭环）]

---

## 四、实验内容与实现

<!-- 
说明：本部分按照 exercises 文档中的练习顺序组织
每个功能模块或者练习中可能包含多项任务，请根据实际情况调整
-->
### 前置实验迁移：Trap 汇编入口、返回与时钟中断

**负责人：** 2410690-王睿瑜

#### 模块功能描述

| 位置 | 实现与理解 |
| --- | --- |
| `trapentry.S` | `__alltraps` 保存现场后把栈上的 `trapframe` 地址作为 C 参数交给 `trap()`。恢复时根据保存的 `sstatus.SPP` 设置 `sscratch`。返回 U 态时为下次陷入预留内核栈地址，返回 S 态时保持内核态入口约定。恢复寄存器和 `sepc` 后执行 `sret`。 |
| 时钟中断 | 识别 S 态时钟中断后设置下一次时钟事件、累加 `ticks`，并在存在当前进程时设置 `need_resched`。从用户态陷入时，`trap()` 在返回用户态前检查此标志并调度。 |


#### 最终提示词

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

#### 实现迭代过程

| 阶段 | 实际现象 | 处理与结果 |
| --- | --- | --- |
| 首次编译检查 | `trapentry.o` 和 `trap.o` 显示 `is up to date`。 | AI 帮助判读日志，确认本次复用了已有文件。 |
| 强制复编译 | 使用 `make -C code -B V= ...`，日志显示实际调用 RISC-V GCC。 | 两个目标均重新编译通过。特权级转换的运行证据待双 GDB 补录。 |

### 功能模块一：系统调用入口与分发

**负责人：** 2410690-王睿瑜

#### 模块功能描述

```c
void exception_handler(struct trapframe *tf);
void syscall(void);
```

本模块同时补充九个静态包装函数，分别对应 `exit`、`fork`、`wait`、`exec`、`yield`、`kill`、`getpid`、`putc` 和 `pgdir`。

| 位置 | 实现与理解 |
| --- | --- |
| `trap.c` | `trap()` 在分发前将当前进程的 `tf` 指向本次现场，分发后恢复原指针。用户态 `ecall` 进入 `CAUSE_USER_ECALL` 分支，先将 `epc` 加 4，再调用 `syscall()`，使返回地址指向下一条指令。 |
| `syscall.c` | 从 `current->tf` 读取系统调用号 `a0` 和参数 `a1`～`a5`。先检查编号范围与表项，再调用对应的 `sys_*` 包装函数。非法编号返回 `-E_INVAL`，结果写回现场的 `a0`，随寄存器恢复传回用户态。 |

当前模块接入了缺页处理接口。对取指、读或写缺页，若 `current->mm` 存在，则把异常号和故障地址交给 `do_pgfault()`。处理成功保留 `epc`，使返回后重试故障指令。处理失败时输出现场，未处理的内核态异常触发 `panic()`，未处理的用户态异常由 `do_exit(-E_KILLED)` 终止相应进程。缺页处理的实际效果由集成后的实现验证。


#### 最终提示词

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

#### 实现迭代过程

| 阶段 | 实际现象 | 处理与结果 |
| --- | --- | --- |
| 接口复核 | 审查时需要确认 `do_pgfault()` 的第二个参数。 | 对照小组契约确认直接传递 RISC-V 异常号，保留现有调用。 |
| 源码与编译核对 | `syscall()` 已有函数体，模块标记仍显示 `YOUR CODE`。VS Code 对若干符号画出红线。 | 按要求核对学号标记，以 WSL 的交叉编译结果判断这些符号是否有编译问题。`syscall.o` 强制重编译通过。 |

### 功能模块四：其余系统调用功能

**负责人：** 2410690-王睿瑜

#### 模块功能描述

```c
int do_exit(int error_code);
int do_yield(void);
int do_kill(int pid);
```

- **`do_exit(error_code)`** 保护 `idleproc` 和 `initproc`。当前进程有用户地址空间时，先切回启动页表并刷新 TLB，再解除对 `mm` 的引用。引用计数归零才释放该地址空间。随后写入退出码，将当前进程设为 `PROC_ZOMBIE`，唤醒处于 `WT_CHILD` 的父进程，并将现有子进程移交给 `initproc`。若移交的子进程已是僵尸，也唤醒等待中的 `initproc`。最后调度其他进程，等待方通过 `do_wait()` 回收进程结构和内核栈。
- **`do_yield()`** 设置 `current->need_resched` 并返回 0，调度在已有的 Trap 返回路径发生。
- **`do_kill(pid)`** 在中断保护下查找目标并更新标志。找不到进程时返回 `-E_INVAL`，已设置 `PF_EXITING` 时返回 `-E_KILLED`。首次设置退出标志后，若目标处于可中断睡眠则唤醒它，使它有机会进入退出路径。


#### 最终提示词

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

#### 实现迭代过程

| 阶段 | 实际现象 | 处理与结果 |
| --- | --- | --- |
| 确定实现位置 | WSL 未安装 `rg`，改用 `grep` 查询，确认 `function4` 标记与后续宏之间为空。 | 用四段式提示词限定 AI 只补模块四。 |
| 审查和复编译 | 新代码包含 `do_exit()`、`do_yield()`、`do_kill()`，并标记学号 2410690。 | 执行 `git diff --check` 并强制编译 `proc.o`，两项通过。进程回收与终止后的等待仍待整机验证。 |


### 分支任务：双 GDB 跟踪系统调用

**负责人：** 2410690-王睿瑜



## 五、测试与验证

<!-- 根据该实验的具体情况，提供完整的测试运行截图，应包含：
- 编译及运行成功的输出（make qemu），可能有多个
- 测试通过的结果（make grade），只有一个
-->

**测试截图：**

![测试结果截图](./images/test_result.png)

---

## 六、实验总结与收获

### 对操作系统的理解

#### 实验知识点与 OS 原理的对应

| 原理知识 | Lab5 中的体现 |
| --- | --- |
| 特权级与系统调用 | 用户态代码通过同步异常请求内核服务。硬件负责陷入所需的 CSR 与特权级转换，内核汇编负责保存和恢复通用寄存器。 |
| Trap 现场与调度上下文 | `trapframe` 保存从异常或中断返回需要的状态。进程的 `context` 保存内核线程切换所需的状态，两者分别用于 Trap 返回和调度切换。 |
| 地址空间生命周期 | 切回启动页表后再释放用户页表，`mm` 引用计数用于确定共享地址空间的最后一个使用者。 |
| 进程结束与回收 | `exit` 记录状态并释放地址空间引用，`wait` 负责最终回收。保留僵尸态让父进程可以在子进程结束后读取退出码。 |
#### OS 原理中重要但没有对应上的知识点

本实验用 `need_resched` 和已有调度路径演示重新调度，用 `PF_EXITING` 表达退出请求。多核负载均衡、实时调度策略，以及 POSIX 信号的屏蔽、挂起与递送还需要额外机制。

### AI 协作开发的经验

提示词中先写明输入来自哪份 Trap 帧，返回值写回哪个寄存器，再写进程状态和资源回收的边界，能够直接与源码和课程测试对照。遇到编辑器诊断时，我以实际编译日志核对，使用强制重编译确认工具链确实处理了修改后的文件。特权级切换与退出后的回收还需要看运行证据。

---

