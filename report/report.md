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

**功能模块二** [2410690-王睿瑜]
1. 用 alloc_proc() 建立 PCB 的基础状态，为启动线程和普通新线程共用。
2. 依据 kernel_thread_entry 和 Trap 恢复代码，确定首次运行需要的函数地址、参数、栈指针和入口地址。
3. 用 do_fork() 串联 PCB 分配、内核栈分配、地址空间接口、现场准备和 PID 登记，最后唤醒新线程。
4. 在 proc_init() 中初始化全局数据结构，把 idleproc 设为 current，然后调用 kernel_thread() 创建 initproc，交给后续调度。
---

## 四、实验内容与实现

<!-- 
说明：本部分按照 exercises 文档中的练习顺序组织
每个功能模块或者练习中可能包含多项任务，请根据实际情况调整
-->

### 功能模块二：[进程创建及初始化]

**负责人：** [2410690-王睿瑜]

#### 模块功能描述

**需要实现/修改的函数：**

修改文件为 code/kern/process/proc.c，对应框架中的 LAB4 function2。本模块实现以下函数。

```c
// 列出本模块涉及的所有函数签名
static struct proc_struct *alloc_proc(void);
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags);
static void copy_thread(struct proc_struct *proc, uintptr_t stack,
                        struct trapframe *tf);
static int copy_mm(uint32_t clone_flags, struct proc_struct *proc);
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf);
void proc_init(void);
```

**功能说明：**

本模块负责内核线程的创建与初始化，为调度器提供可运行的线程。系统启动时，`proc_init()` 将当前执行现场登记为 `idleproc`，使其复用启动栈并成为当前线程，再通过 `kernel_thread()` 创建执行 `init_main()` 的 `initproc`。

创建新线程时，`do_fork()` 分配进程控制块（PCB）和内核栈，准备首次运行所需的 Trap 帧与切换上下文，再分配 PID，将线程登记到进程链表和 PID 哈希表，并置为就绪态。如果创建过程中发生错误，函数返回相应错误码，并释放已经分配的资源。

本模块调用内存管理模块的接口分配 PCB 和内核栈，创建的内核线程共享启动页表。调度模块选中新线程后，通过 `switch_to()` 恢复切换上下文，从 `forkret()` 进入 Trap 恢复流程，最终执行创建时指定的线程函数。

#### 最终提示词

以下是经过迭代优化后，最终成功实现该功能的提示词：

````markdown

[TASK]
基于小组 Lab4 框架，协助我逐步完成 code/kern/process/proc.c 的 LAB4 function2。
先核对实际源码和函数依赖，再按 PCB 初始化、首次运行现场、创建与登记、启动初始化的顺序给出实现。


[RELY]
以 proc.h 的 proc_struct 和 context、trap.h 的 trapframe 和 pushregs 为准。
依据 entry.S 确认 s0 保存线程函数地址，s1 保存参数。依据 trapentry.S 确认
forkrets 将 sp 指向传入的 Trap 帧，随后通过 __trapret 恢复现场并执行 sret。
使用框架已有的 boot_pgdir_pa、bootstack、KSTACKSIZE、CLONE_VM、
setup_kstack、put_kstack、get_pid、hash_proc、wakeup_proc 和中断保存/恢复接口。
从 riscv.h 使用 read_csr 和 SSTATUS 位定义。

[SPECIFICATION]
实现 alloc_proc、kernel_thread、copy_thread、copy_mm、do_fork 和 proc_init。
新 PCB 处于 PROC_UNINIT，PID 为 -1，使用启动页表。新线程的 Trap 帧位于
自己的内核栈上，首次切换进入 forkret。当前阶段保留 copy_mm 接口。
分配失败时返回负错误码并释放已取得的资源。PID 分配、登记和计数更新置于
同一个关中断区间，登记完成后再唤醒。先将 idleproc 设为 current，再创建 initproc。
给出逐步验证方法。若整机构建受其他模块阻塞，先用 Makefile 单独编译 proc.o，
并根据实际终端输出记录验证状态。

````

#### 实现迭代过程

本模块的实现经历了 [3] 次迭代，过程如下：

---

##### 第一次迭代

**遇到的问题：**
最初按 alloc_proc() 的函数签名搜索源码，终端没有返回结果。查看完整的 proc.c 后发现，框架只提供了 LAB4 function2 YOUR CODE 标记，需要在该位置新增函数。

**问题解决策略：**
在提示词的 [RELY] 中补充实际的 proc.c 和 proc.h，明确以 LAB4 function2 标记定位实现区域。根据 proc_struct 的实际字段编写 alloc_proc()，并核对 boot_pgdir_pa 和 list_init() 的声明后再初始化 PCB。

##### 第二次迭代

**遇到的问题：**
1. 仅看 C 语言接口，无法确定新线程首次运行时，函数地址和参数应放在哪些寄存器里。

2. 审查 set_proc_name() 时发现，它会从传入地址固定复制 PROC_NAME_LEN 个字节。直接传入 "idle" 或 "init" 会读出字符串字面量的边界。

**问题解决策略：**
1. 在 [RELY] 中补充 entry.S、trapentry.S 和 trap.h。根据 move a0, s1 和 jalr s0，在 kernel_thread() 中将函数地址存入 s0、参数存入 s1。在 copy_thread() 中把 Trap 帧复制到新线程的内核栈，并将切换上下文的返回地址设为 forkret。

2. 在 [SPECIFICATION] 中明确线程名称按实际字符串长度复制，并包含结尾的 \0。在 proc_init() 中分别使用 sizeof("idle") 和 sizeof("init") 作为复制长度。

##### 第三次迭代

**遇到的问题：**
1. 执行 make -C code 时，编译在 kern/trap/trap.c 处中止。trap() 调用 interrupt_handler() 和 exception_handler() 时，编译器在调用处看不到这两个函数的声明。整机构建尚未编译到 proc.c。

**问题解决策略：**
1. 在提示词中补充验证步骤。整机构建受其他模块阻塞时，先通过项目 Makefile 单独构建 B 模块，再用 -B 强制重新编译，确认结果来自本次构建。实际执行了 make -B -C code obj/kern/process/proc.o，编译通过。


**最终结果：**

经过 [3] 次迭代，该模块最终：
- ✅ 完成 alloc_proc()、kernel_thread()、copy_thread()、copy_mm()、do_fork() 和 proc_init() 的实现。
- ✅ git diff --check 检查通过，proc.c 单独编译及强制重新编译均通过。
- ✅ 代码已推送至 lab4-wangruiyu 分支，提交号为 058c72e。
- ⏳ initproc 首次运行、资源分配失败后的清理以及整机评分仍待实测。

**关键改进点总结（如有）：**
1. 在 [RELY] 中补充实际的源码标记、数据结构和汇编入口，明确 C 代码与汇编之间的寄存器约定。
2. 在 [SPECIFICATION] 中明确 Trap 帧的位置、线程名称的复制长度，以及 do_fork() 在不同失败阶段需要释放的资源。
3. 通过 Makefile 单独验证 proc.o，并将首次运行和失败清理保留为后续的运行验收项。


### 练习：[练习标题]

**负责人：** [学号-姓名]

[按照练习的具体要求进行解答]

---

### Challenge：[Challenge标题]

**负责人：** [学号-姓名]

[按照Challenge的具体要求进行解答]

---

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

| 知识点 | 在本实验中的体现 |
|---|---|
| PCB 与状态转换 | `proc_struct` 保存 PID、状态、内核栈和上下文等信息。新 PCB 从 `PROC_UNINIT` 开始，由 `wakeup_proc()` 进入 `PROC_RUNNABLE`，后续由调度器选择。 |
| 线程资源共享 | 两个内核线程可以共享内核地址空间，同时保有各自的 PCB 和栈。这里 `idleproc` 复用启动栈，`initproc` 使用单独分配的内核栈。 |
| 切换上下文与 Trap 返回 | `context` 用于恢复内核切换所需的寄存器，`trapframe` 用于恢复完整的 Trap 返回现场。首次运行通过预先构造这两份状态，把新线程送到指定入口。 |
| 进程标识和并发保护 | PID 分配和链表、哈希表登记在同一段本地关中断区间内完成，防止本 CPU 的中断路径在登记过程中观察到不一致状态。 |

### AI 协作开发的经验

这次先用 AI 梳理每个函数应满足的条件，再对照实际源码逐项确认。最需要核对的是 C 和汇编的接口。只看函数名无法确定首次运行时函数地址和参数放在哪些寄存器里，读到 move a0, s1 和 jalr s0 后，才能准确设置 Trap 帧。整机构建受阻时，单独构建目标文件可以确认自己的模块能编译。首次切换和失败清理还需要对应的运行证据。
---

