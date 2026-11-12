# Lab 4 提示词汇总

## 功能模块三：进程调度

**负责人：** 2411333-吴卓彦

### 一、Spec（设计依据）

**1. 输入与调用条件**

- `cpu_idle()` 与 `schedule()` 依赖初始化后的 `current`、`idleproc` 和带哨兵的循环进程链表。普通线程已通过 `list_link` 挂入链表。进入调度前，本地中断可以开启或关闭。
- `proc_run(proc)` 接收合法的目标 PCB，其 `pgdir` 是页对齐的页表根物理地址，目标上下文已准备好。
- `switch_to(from, to)` 通过 `a0`、`a1` 接收上下文地址，使用 RV64 下的 14 个寄存器槽位。已迁移的 Trap 恢复路径提供新线程首次运行时所需的 `__trapret`。

**2. 输出与处理过程**

- `cpu_idle()` 持续检查 `need_resched`，在标记置位时调用 `schedule()`。
- `schedule()` 在关中断期间清除调度标记。当前是 idle 时从链表头之后开始，否则从当前节点之后开始。跳过哨兵和非就绪线程，绕回时可选仍然就绪的当前普通线程。无就绪普通线程时回到 idle。每次给选中线程增加一次 `runs`，目标不同时才切换。
- `proc_run()` 在临界区更新 `current`，写入 Sv39 `satp`、刷新 TLB，再调用 `switch_to()`。目标就是当前线程时直接返回。旧线程后来从切换调用处恢复时，还原它先前保存的本地中断状态。
- `switch_to()` 按 `struct context` 的顺序保存、恢复 `ra`、`sp`、`s0`～`s11`。新线程经 `forkret → forkrets → __trapret → kernel_thread_entry` 首次执行，旧线程从上次切出的调用位置继续。

**3. 修改范围与验收**

- 仅实现 `code/kern/process/proc.c`、`code/kern/schedule/sched.c` 和 `code/kern/process/switch.S` 中的 `LAB4 function3`。保留框架接口、进程创建代码和已迁移的 Trap 代码。按报告要求用本人学号替换 `YOUR CODE`。
- 对照源码审查链表边界、开关中断及寄存器布局。由实验者在集成后亲自运行 `make qemu` 和 `make grade`，确认 `initproc` 输出及 `[Test Schedule] PASS`。

### 二、最终提示词

生成时打开了 `Lab4调度实现提示词.md`，并输入「直接按照spec修改代码」。下方保留工作区中的该提示词正文。截图记录了执行指令、改动和 AI 的回复。

```text
# Lab 4 进程调度实现提示词

[PROMPT]

**任务**

在项目 `E:\大三上\OS\OS2026_Labs` 的 `Lab4` 分支上实现 uCore 的进程调度。目标平台为 RISC-V RV64，分页模式为 Sv39。请先阅读下列依赖，再按规格直接修改项目文件。

**操作要求**

| 允许修改的文件 | 修改范围 |
| --- | --- |
| `code/kern/process/proc.c` | 在 `LAB4 function3` 标记处实现 `cpu_idle` 和 `proc_run`，按需引入框架已有头文件。 |
| `code/kern/schedule/sched.c` | 在 `LAB4 function3` 标记处实现 `schedule`，保留已有的 `wakeup_proc`。 |
| `code/kern/process/switch.S` | 在 `LAB4 function3` 标记处实现并导出 `switch_to`。 |

保留实验标记，`YOUR CODE` 填写学号`2411333`。沿用框架的公共声明和结构体布局。编辑范围限于上表，不修改进程创建代码、Trap 代码、内存管理、构建配置和测试。

**输出要求**

实际修改工作区源码，完成后只说明修改了哪些文件和函数、还有哪些点需要实验者核对。编译、QEMU、评分和截图由实验者完成。请勿在 WSL 编译、运行 QEMU、执行评分或生成测试证据。

[RELY]

- `code/kern/process/proc.h` 和 `code/kern/process/proc.c` 提供 PCB、`struct context`、`current`、`idleproc`、`proc_list` 和 `le2proc`。普通线程通过 `list_link` 登记到进程链表。内核线程的 `pgdir` 是页表根的物理地址。
- `code/kern/schedule/sched.h` 提供调度接口。 `code/libs/list.h` 中的双向循环链表以 `proc_list` 为哨兵。使用 `list_next` 遍历，对普通节点使用 `le2proc(node, list_link)`。
- `code/kern/sync/sync.h` 和 `code/kern/driver/intr.c` 提供本 hart 的中断保存与恢复接口。`local_intr_save` 记录进入前的 `sstatus.SIE` 状态并关中断，`local_intr_restore` 按保存值恢复。
- `code/libs/riscv.h` 提供 CSR 操作、`SATP_MODE_SV39`、`RISCV_PGSHIFT` 和 `REGBYTES`、`LOAD`、`STORE` 等汇编宏。对照 `code/kern/mm/pmm.c` 的 `enable_paging()` 和 `code/kern/mm/pmm.h` 的 `flush_tlb()`。现有 `lsatp()` 适用于 Sv32。
- 新线程的 `context` 和 trapframe 由线程创建代码准备，首次运行沿用 `forkret → forkrets → __trapret → kernel_thread_entry`。

[GUARANTEE]

- 交付 `void cpu_idle(void)`、`void schedule(void)`、`void proc_run(struct proc_struct *proc)`、`void switch_to(struct context *from, struct context *to)` 的实现，保持框架接口可调用。
- 调度覆盖空链表、非就绪节点、绕回、只有当前线程可运行、无可运行普通线程这几种情况。每次调度只计一次 `runs`，选中当前线程时不切换上下文。
- 可运行的新线程能进入已有首跑路径，已经运行过的线程被重新选中时能从原切出位置继续，临界区退出时恢复调用前保存的中断状态。
- 验收由实验者在模块集成后用 `make qemu` 和 `make grade` 完成。检查 `[Test Schedule] PASS` 和 `initproc` 的输出。`code/tools/test_lab4.c` 的调度用例创建两个测试线程并确认两者都实际运行。

[SPECIFICATION]

### 实现单元一 · `cpu_idle(void)`

**Pre-Condition**

- 内核已完成 `proc_init`，`current` 指向有效的 `idleproc`。

**Post-Condition**

- 函数持续运行。仅当 `current->need_resched` 置位时调用 `schedule()`，恢复到空闲线程后继续检查。

### 实现单元二 · `schedule(void)`

**Pre-Condition**

- `proc_list` 已初始化为带哨兵的循环链表。`current`、`idleproc` 有效。若当前是普通线程，它的 `list_link` 已登记在链表中。调用时本地中断可以开启或关闭。

**Post-Condition**

- 保存本地中断状态并关中断，清除调用时当前线程的 `need_resched`。
- 当前是 idle 时从哨兵之后开始，否则从当前线程之后开始。最多扫描一圈，跳过哨兵和非 `PROC_RUNNABLE` 的节点。绕回后可以选中仍然就绪的当前普通线程。找不到可运行普通线程时选择 `idleproc`。
- 选中者的 `runs` 加一。仅当它与 `current` 不同时调用 `proc_run(next)`。本次调用恢复执行并离开临界区时，恢复保存的本地中断状态。

### 实现单元三 · `proc_run(struct proc_struct *proc)`

**Pre-Condition**

- `proc` 和 `current` 都指向有效 PCB。目标的 `pgdir` 是页对齐的物理地址，目标上下文可被恢复。

**Post-Condition**

- 若目标就是 `current`，直接返回，不改写 `satp` 或切换上下文。
- 若目标不同，保存并关闭本地中断，保留旧 `current`，将 `current` 更新为目标。将目标页表根物理地址右移 `RISCV_PGSHIFT` 得到物理页号，与 RV64 Sv39 模式字段组合后写入 `satp`，随后执行 `flush_tlb()`。
- 用旧线程和目标线程的 `context` 地址调用 `switch_to`。旧线程后来恢复执行、从这次调用返回时，恢复它进入临界区前保存的中断状态。

### 实现单元四 · `switch_to(struct context *from, struct context *to)`

**Pre-Condition**

- 按 RISC-V 调用约定，`a0`、`a1` 分别指向可写的旧上下文和已准备好的目标上下文。槽位与 `proc.h` 的 `struct context` 一致，RV64 下每槽 8 字节。

**Post-Condition**

- 将 `ra`、`sp`、`s0`～`s11` 按结构体顺序保存至 `from`，再从 `to` 恢复对应的 14 个槽位。
- 通过恢复后的 `ra` 返回。目标线程首次运行时返回到 `forkret`，已经运行过时回到它上次切出的调用位置。
```

### 三、实现迭代过程

1. **前置迁移。** 先补齐 Lab 3 的 Trap 入口、现场恢复和中断与异常处理。保留时钟重设、计数和周期输出，去掉固定关机规则。
2. **首次生成。** AI 依据独立提示词，在 `proc.c`、`sched.c` 和 `switch.S` 中一轮完成四个调度函数。静态审查确认调度逻辑和上下文布局符合 Spec。

**最终结果**

- ✅ 四个调度函数已实现，补丁格式检查通过。
- ⏳ 各模块合并后统一运行 `make qemu` 和 `make grade`。

**关键改进点总结**

1. 在 `[RELY]` 中说明现有链表、开关中断接口、Sv39 页表和上下文布局，明确实现依赖。
2. 在 `[SPECIFICATION]` 中为四个函数分别写前置条件和后置条件，具体说明绕回、idle 回退、同线程不切换和中断恢复。

![调度代码生成记录](./images/lab4-schedule-ai-generation.png)

