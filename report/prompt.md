# Lab 4 提示词汇总

## 功能模块一：虚拟内存页操作

**负责人：** 2413497-姚嘉乐

### 一、Spec（设计依据）

**1. 输入与调用条件**

- 先根据 `pmm.c` 中的 `get_pte()` 和 `pmm.h` 的接口声明确定输入。`pgdir` 指向 Sv39 根页表，`la` 可以带页内偏移，两个函数操作的是该地址所在的 4 KiB 页。`page_insert()` 接收调用者已取得的物理页描述符和权限位。
- `get_pte()` 返回的是末级项的地址。查表路径缺失和末级项的 `PTE_V` 未置位是两种状态。插入时允许按需分配下级页表，删除时只查找已有路径。

**2. 输出与处理过程**

- 按末级项原来的状态列出插入的预期结果。空项新增一次引用，同页重映射只更新权限，异页替换把引用从旧页移到新页。分配所需的页表页失败时返回 `-E_NO_MEM`，传入数据页仍由调用者持有。
- 对有效映射执行删除时，先撤销映射并刷新该地址的 TLB，再归还引用归零的物理页。路径缺失、末级项为空和重复删除都直接返回。覆盖旧映射时按本次 `perm` 构造新项，以清除被撤销的权限位。

**3. 修改范围与验收**

- 修改范围是 `pmm.c` 中的 `LAB4 function1`。沿用现有页表遍历和物理页分配器，补齐 `page_insert()` 和 `page_remove()`。
- 整合使用make qemu和make grade做全面的检查，过程再重点核对同页重映射时空闲页数是否保持原值，以及重复删除是否只撤销一次引用。

### 二、最终提示词

按 Lab0.5 的四段式组织提示词。`[PROMPT]` 写明实现位置、修改范围和学号标记，`[RELY]` 列出查表、页号转换、引用计数和 TLB 接口，`[GUARANTEE]` 给出两个函数签名。`[SPECIFICATION]` 为两个函数分别写前置条件、后置条件和各个分支的预期结果。

```text
[PROMPT]
任务：
在code/kern/mm/pmm.c中实现 page_insert() 和 page_remove()，用于建立、替换和撤销 Sv39 的 4 KiB 页映射。

操作要求：
在工作区的实际源码中补全 //LAB4 function1 YOUR CODE 所在区域，将标记改为 //LAB4 function1 2413497。编辑范围限于该区域，沿用现有函数声明、页表查找逻辑和物理页分配器。需要共用的辅助函数可以定义为本文件的 static 函数。

输出要求：
直接进行文件操作，使用下面[RELY]、[GUARANTEE]、 [SPECIFICATION]部分的信息。积极合理的使用注释，维护代码的可读性。

[RELY]
请先阅读code/kern/mm/pmm.c和code/kern/mm/pmm.h。

- 查找页表项。get_pte(pgdir, la, 1) 可以创建缺少的下级页表，传 0 则只查找。
  返回 NULL 表示没找到末级项。返回指针时，用 *ptep & PTE_V 判断该项是否有效。
  只读查询也可以用 get_page()，它返回已映射的物理页描述符，并可通过ptep_store 输出末级页表项的地址。
- 读写映射。用 pte_create(page2ppn(page), perm) 构造新页表项，它会设置 PTE_V。用 pte2page(*ptep) 取得有效页表项对应的旧页。
- 管理物理页。page_ref_inc() 和 page_ref_dec() 更新引用计数。
  page_ref_dec() 返回减一后的计数，free_page() 将一页归还分配器。
- 修改末级页表项后，调用 tlb_invalidate(pgdir, la)。
- 常量。PTE_V 定义在 mmu.h，E_NO_MEM 定义在 error.h。

[GUARANTEE]

实现以下两个公开接口，签名与 pmm.h 一致。

int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm);
void page_remove(pde_t *pgdir, uintptr_t la);

[SPECIFICATION]

实现单元一  page_insert()

Pre-Condition
物理内存管理已初始化。pgdir 指向有效的 Sv39 根页表。la 位于可使用末级页表项的 4 KiB 映射范围内。page 是已分配或已被其他地址映射的物理页。

Post-Condition
成功时返回 0。目标虚拟页指向 page，页表项按传入的 perm 构造。被撤销的旧映射及其引用计数得到正确处理。

Case 1  末级项原本为空
建立映射，目标物理页的引用计数增加一次。

Case 2  原映射指向同一物理页
更新页表项的权限。物理页号、引用计数和空闲页数保持原值。

Case 3  原映射指向另一物理页
新页增加一次引用，旧页减少一次引用。旧页仍有引用时继续保留，归零时归还分配器。

Case 4  无法分配所需的页表页
返回 -E_NO_MEM。目标数据页的映射和引用计数均未改变，调用者仍负责传入的 page。get_pte() 已挂接的上层页表沿用框架的管理方式。

Requirements
每次成功写入末级项后，同步该虚拟地址的 TLB。回收旧页前，确保旧页表项和旧翻译均已失效。权限按 perm 重新构造，旧权限中被撤销的位应清除。

实现单元二  page_remove()

Pre-Condition
pgdir 指向有效的 Sv39 根页表。la 位于可使用末级页表项的 4 KiB 映射范围内，目标映射可以存在或缺失。

Post-Condition
目标虚拟页的映射被撤销。其他虚拟页的映射和上层页表页继续有效。

Case 1  页表路径缺失或末级项无效
直接返回，页表、引用计数和空闲页数保持原值。对同一地址重复删除也应满足这个行为。

Case 2  末级项有效
先取得旧物理页，再清空末级项并同步该地址的 TLB。旧页引用减少一次，计数归零时调用 free_page()。
```

## 功能模块二：进程创建及初始化

**负责人：** 2410690-王睿瑜

### 一、Spec（设计依据）

**1. 输入与调用条件**

- 根据 `proc.h` 确定 PCB 和切换上下文的字段，根据 `trap.h` 确定 Trap 帧布局。`proc_init()` 在内存管理初始化完成后建立进程管理结构，`do_fork()` 依赖已有的 `current` 和登记结构。
- 对照线程入口的 `move a0, s1` 和 `jalr s0`，确定 `s0` 保存线程函数地址，`s1` 保存参数。对照 `forkrets` 和 `__trapret`，确定新线程的 Trap 帧放在它自己的内核栈上，切换上下文从 `forkret` 开始。

**2. 输出与处理过程**

- `alloc_proc()` 建立初始 PCB，状态为 `PROC_UNINIT`，PID 为 `-1`，页表根使用 `boot_pgdir_pa`。本阶段的内核线程共享内核地址空间，`copy_mm()` 保留接口。
- `do_fork()` 先准备 PCB、内核栈和首次运行现场，再在关中断区间内分配 PID、登记链表和哈希表、更新进程数。登记完成后唤醒新线程并返回 PID。分配失败时释放已取得的资源，返回对应的负错误码。
- `proc_init()` 先把复用启动栈的 `idleproc` 设为 `current`，再通过 `kernel_thread()` 创建运行 `init_main()` 的 `initproc`。

**3. 修改范围与验收**

- 修改范围是 `proc.c` 中的 `LAB4 function2`，实现 PCB 分配、线程现场准备、创建与登记以及启动初始化所需的六个函数。
- 整机构建受其他模块阻塞时，先用 `make -B -C code obj/kern/process/proc.o` 检查本模块能否编译。集成后用 `make qemu` 核对 `initproc` 的输出，并通过 `make grade` 的进程创建用例检查两个线程的初始状态。

### 二、最终提示词

实际提示词用 `[TASK]` 确定修改位置和实现顺序，`[RELY]` 列出结构体、汇编入口和已有辅助函数。`[SPECIFICATION]` 同时列出六个实现接口，并说明首次运行现场、资源分配失败、登记和唤醒的要求。为便于在其他模块未完成时验证，还写明了单独编译 `proc.o` 的方式。

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

