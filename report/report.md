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

以下是首轮在 VS Code 中发送给 Codex 的完整提示词。第二轮的修正提示词见 Lab4-B-prompt.md。

````markdown

[PROMPT]
任务
你正在操作 kylie-dot/OS2026_Labs 的 RISC-V uCore 教学内核。请在当前工作区直接修改 code/kern/process/proc.c，一次完成 Lab4 功能模块二“进程创建及初始化”。工作区应基于小组的 Lab4 框架提交 a211d8d，目标文件应有 LAB4 function2 YOUR CODE 标记。先检查当前分支、工作区和文件内容。如果当前版本不是该框架、模块已经实现，或目标区域含有他人的改动，请说明情况并停下，保留已有工作。

操作要求
在 function2 标记下新增下列六个函数，将标记改为 //LAB4 function2 2410690。
仅编辑 proc.c 的 function2 区域及该文件必要的 include 和 extern 声明。保留 function3 标记、现有辅助函数、结构体、公共签名、Trap 与调度实现、测试和构建脚本。优先使用项目现有接口。

输出要求
直接编辑实际文件并审查 diff。完成后说明修改位置、线程首次运行路径、失败清理顺序以及仍需验证的项目。编译、QEMU、评分与截图由我完成。不要代为提交或推送 Git，也不要声称未经执行的测试通过。

[RELY]
从仓库根目录读取：
- code/kern/process/proc.c、proc.h、entry.S
- code/kern/trap/trap.h、trapentry.S
- code/kern/schedule/sched.c、sched.h，及 switch_to 的实际汇编实现或待实现位置
- code/kern/mm/pmm.h、memlayout.h、kmalloc.h
- code/kern/sync/sync.h、code/libs/riscv.h、list.h、error.h
- code/kern/init/init.c、entry.S 和 code/tools/test_lab4.c

以实际声明核对 bootstack、boot_pgdir_pa、KSTACKSIZE、CLONE_VM、MAX_PROCESS、错误码和中断保存/恢复接口。使用已有的 setup_kstack、put_kstack、get_pid、hash_proc、find_proc、wakeup_proc 与 forkret。核对 entry.S 中 move a0, s1 和 jalr s0，以及 forkrets 将传入指针交给 __trapret 的约定。需要读取 sstatus 时显式引入声明 read_csr 和 SSTATUS 位的头文件。

[GUARANTEE]
实现以下函数并保留给定接口：
static struct proc_struct *alloc_proc(void);
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags);
static void copy_thread(struct proc_struct *proc, uintptr_t stack, struct trapframe *tf);
static int copy_mm(uint32_t clone_flags, struct proc_struct *proc);
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf);
void proc_init(void);

[SPECIFICATION]
实现单元：alloc_proc
Pre-Condition：kmalloc 与启动页表已就绪。
Post-Condition：分配失败返回 NULL。成功时初始化全部 PCB 字段，设置 state=PROC_UNINIT、pid=-1、pgdir=boot_pgdir_pa，mm、parent、tf 等指针为空。运行次数、栈地址、调度标记、flags、名称和 context 清零。分别初始化 list_link 与 hash_link，PCB 此时未登记到全局结构。

实现单元：kernel_thread
Pre-Condition：fn 是有效的内核线程入口，调用方已建立 current。
Post-Condition：创建并清零临时 Trap 帧，s0 存 fn、s1 存 arg，epc 指向 kernel_thread_entry。根据当前 sstatus 设置 SPP 和 SPIE、清除 SIE，使 sret 回到 S 模式。调用 do_fork(clone_flags | CLONE_VM, 0, &tf)，返回其结果。

实现单元：copy_thread
Pre-Condition：新 PCB 的内核栈已由 setup_kstack 分配，tf 指向有效模板。
Post-Condition：在新内核栈顶部预留并复制完整 Trap 帧，令 proc->tf 指向副本。将副本的 a0 置为 0。stack 为 0 时，将副本的 gpr.sp 设为帧地址；否则使用 stack。context.ra 指向 forkret，context.sp 指向帧地址。首次切换应能走 forkret → forkrets → __trapret → sret → kernel_thread_entry。

实现单元：copy_mm
Pre-Condition：当前实验创建内核线程，PCB 已完成默认初始化。
Post-Condition：保留接口并返回 0，内核线程继续共享启动页表，mm 保持 NULL。避免新增用户地址空间复制逻辑。

实现单元：do_fork
Pre-Condition：current、进程链表和 PID 哈希桶已经初始化，tf 指向有效模板。
Post-Condition：
1. 达到 MAX_PROCESS 时返回 -E_NO_FREE_PROC；PCB 分配失败时返回 -E_NO_MEM。
2. 设置父进程为 current，依次准备内核栈、地址空间接口和运行现场。内核栈分配失败时释放 PCB。copy_mm 失败时先释放内核栈再释放 PCB，返回对应错误。失败路径不能将 PCB 留在全局结构中或增加进程计数。
3. 在同一个本地关中断区间内分配唯一 PID、登记哈希表和 proc_list，并更新 nr_process。按原状态恢复中断。完成登记后调用 wakeup_proc，返回新 PID。

实现单元：proc_init
Pre-Condition：由启动路径调用一次，bootstack 与启动页表已建立。
Post-Condition：初始化进程链表和全部哈希桶。创建 idleproc，设 PID 为 0、状态为 PROC_RUNNABLE，复用 bootstack，置 need_resched，计入 nr_process 并设为 current。idleproc 无需进入普通 PID 哈希表。再调用 kernel_thread 创建运行 init_main 的 initproc；创建失败须按框架方式报错。用返回的 PID 通过 find_proc 取回 initproc 并设置名称。线程名称为 idle 和 init，复制时包含结尾 NUL，避免用固定长度从短字符串字面量越界读取。演示参数使用 Hello world!!，若框架测试明确指定其他参数，以测试要求为准。

````

#### 实现迭代过程

本模块的实现经历了 [2] 次迭代，过程如下：

---

##### 第一次迭代

**遇到的问题：**
1. Codex 无法解析独立工作区的 .git worktree 指针，因此无法自行核对 Git 基线和变更范围。

2. 首轮生成的代码通过了单文件编译，但人工审查发现，kernel_thread() 仅在 SSTATUS_SPP 已经为 1 时才再次置位。若读出的 SPP 为 0，首次执行 sret 时就无法保证返回 S 模式。

**问题解决策略：**
1. 在 VS Code 中打开基于框架提交 a211d8d 创建的独立工作区，将完整提示词一次性发送给 Codex。Codex 根据 proc.c 中的 LAB4 function2 YOUR CODE 标记完成六个函数的实现，随后由本人在 WSL 中核对 Git 基线和修改范围。

2. 执行 git diff --check 和 make -B -C code obj/kern/process/proc.o，检查补丁格式并强制重编译目标文件。编译通过后，对照首次运行所需的寄存器状态审查代码，将 SPP 的条件置位问题列为下一轮修正项。

##### 第二次迭代

**遇到的问题：**
1. 需要修正 kernel_thread() 中的 sstatus 初始化，保证新建内核线程首次执行 sret 时进入 S 模式。

2. 首轮生成的 proc_init() 将 idleproc 加入了 proc_list，需要结合 PID 分配、调度和测试代码核对这一处理。


**问题解决策略：**
1. 向 Codex 发送修正提示词，限定修改范围为 proc.c 的 function2 区域。Codex 将条件置位改为 tf.status |= SSTATUS_SPP;，同时保留 SPIE 置位与 SIE 清零。

2。 要求 Codex 对照 get_pid()、调度器和 test_lab4.c 复核 idleproc 的登记方式。复核后保留 idleproc 在 proc_list 中的登记，PID 哈希表中未登记 idleproc。这一处理仍需结合整机调度验证。




**最终结果：**

**最终结果**

经过 2 次 Codex 迭代，完成了 `alloc_proc()`、`kernel_thread()`、`copy_thread()`、`copy_mm()`、`do_fork()` 和 `proc_init()`。第二轮将 `SSTATUS_SPP` 改为无条件置位，修正版用于正式提交 `40e9808`。

在 WSL 中执行 `git diff --check`，未发现补丁格式问题。执行 `make -B -C code obj/kern/process/proc.o`，强制重编译通过。QEMU 首次调度和 `make grade` 结果待小组合并后补充。



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

