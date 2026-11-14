# 操作系统实验报告

## 实验基本信息

| 项目 | 内容 |
|------|------|
| **实验名称** | Lab 4：进程管理 |
| **小组成员** | 2411333-吴卓彦、2410690-王睿瑜、2413497-姚嘉乐 |
| **完成日期** | 2026-11-13 |

### 小组分工

| 成员 | 负责的练习/模块 |
|------|----------------|
| 2413497-姚嘉乐 | 功能模块一：虚拟内存页操作，迁移 Lab2 的物理内存初始化和 Best-Fit，完成 Challenge 2（深入理解不同分页模式的工作原理） |
| 2410690-王睿瑜 | 功能模块二：进程创建及初始化，负责 PCB 初始化、内核线程创建和首次运行现场的准备，深入了解线程创建主线，完成PCB、内核栈、trapframe 的原理对照 |
| 2411333-吴卓彦 | 功能模块三：进程调度与上下文切换，迁移 Lab3 的 Trap 处理代码，完成 Challenge 1（开关中断的实现）|

报告第三，四，六节由各成员撰写自己负责的模块和扩展练习，五、测试与验证由2411333-吴卓彦完成，最终报告整合由2413497-姚嘉乐完成。

---

## 一、实验目的

1. 理解 Sv39 多级页表的组织方式，实现 4 KiB 页映射的建立、替换和撤销，掌握页表项、物理页引用计数和 TLB 同步之间的关系。
2. 理解内核线程的创建和初始化过程，掌握 PCB、内核栈和 Trap 帧的用途，以及 `idleproc` 和 `initproc` 的建立过程。
3. 理解调度器如何选择就绪线程，掌握切换时如何更新当前线程、切换页表并保存和恢复寄存器。通过 QEMU 观察 `initproc` 首次运行。
4. 按四段式编写 Spec 和提示词，借助 AI 完成代码实现，并通过代码审查、QEMU 启动和课程评分验证结果。

---

## 二、实验环境

**AI 编程工具与模型**

| 成员 | AI 编程工具 | 底层模型 |
|------|------------|----------|
| 2413497-姚嘉乐 | Codex（VS Code 插件） | GPT-6.1-sol |
| 2411333-吴卓彦 | Claude Code（CLI） | GPT-6.1-sol |
| 2410690-王睿瑜 | Codex（VS Code 插件） | GPT-6.1-sol |


---

## 三、实验整体逻辑分析

### 3.1 本章节的逻辑主线

本实验从内核启动时的一条执行流出发，先准备内存管理和 Trap 恢复机制，再建立内核线程并完成第一次调度。启动汇编先用 Sv39 大页建立内核的高地址映射。进入 `kern_init()` 后，内核初始化物理页分配器，验证 4 KiB 页映射操作，并准备分配 PCB 和内核栈所需的内存。

`proc_init()` 把正在执行的启动现场登记为 `idleproc`，并创建共享启动页表、拥有独立内核栈的 `initproc`。此时 `initproc` 已就绪，CPU 仍在执行 `idleproc`。进入 `cpu_idle()` 后，调度器才选中 `initproc`。上下文切换恢复它预先准备好的栈和返回地址，再借助 Lab3 的 Trap 恢复代码进入线程入口，最终执行 `init_main()`。

```text
启动入口 → kern_init()
         → pmm_init() → pic_init() → idt_init() → vmm_init()
         → proc_init()：登记 idleproc，创建 initproc
         → clock_init() → intr_enable()
         → cpu_idle() → schedule() → proc_run() → switch_to()
         → forkret → forkrets → __trapret（sret）
         → kernel_thread_entry → init_main()
```

### 3.2 功能的逐步实现

首先实现功能模块一、虚拟内存页操作。在 Lab2 物理页管理的基础上，补全 page_insert() 和 page_remove()，通过页表自检后继续初始化内核内存分配器，为线程创建准备所需的内存。

接着实现功能模块二、内核线程的创建与初始化。建立 idleproc，为 initproc 分配 PCB 和内核栈，准备首次运行的现场，并将其置为就绪状态，供调度器选择。

最后实现功能模块三、线程调度与上下文切换。调度器选中就绪的 initproc，借助上下文切换和 Lab3 的 Trap 恢复代码，让它开始执行 init_main()。

实际编程中，为了便于团队协作，我们按这三个模块并行开发，完成后统一整合。


**功能模块一（2413497-姚嘉乐）**

1. **接回物理页管理。** 迁移 Lab2 的 `page_init()` 和 Best-Fit 分配器，探测物理内存、初始化页描述符并登记空闲页。后续新增下级页表和分配内核栈都依赖物理页分配器。
2. **补齐页映射操作。** 在已有的 `get_pte()` 上实现 `page_insert()` 和 `page_remove()`，处理映射的建立、替换与撤销，同时维护物理页引用计数和 TLB。内核在 `pmm_init()` 中通过 `check_pgdir()` 和 `check_boot_pgdir()` 检查页表行为，再初始化供 PCB 分配使用的 `kmalloc`。

**功能模块二** [2410690-王睿瑜]
1. 用 alloc_proc() 建立 PCB 的基础状态，为启动线程和普通新线程共用。
2. 依据 kernel_thread_entry 和 Trap 恢复代码，确定首次运行需要的函数地址、参数、栈指针和入口地址。
3. 用 do_fork() 串联 PCB 分配、内核栈分配、地址空间接口、现场准备和 PID 登记，最后唤醒新线程。
4. 在 proc_init() 中初始化全局数据结构，把 idleproc 设为 current，然后调用 kernel_thread() 创建 initproc，交给后续调度。

**功能模块三（2411333-吴卓彦）**

1. **触发调度。** `proc_init()` 完成线程初始化后，`cpu_idle()` 检查 `need_resched`，置位时调用 `schedule()`。
2. **选择线程。** `schedule()` 沿进程链表查找就绪线程，选定下一次运行的线程。
3. **切换现场。** `proc_run()` 更新 `current` 和页表，`switch_to()` 保存旧线程的寄存器并恢复新线程的寄存器。
4. **首次运行。** `initproc` 经 `forkret → forkrets → __trapret → kernel_thread_entry` 进入 `init_main()`。

---

## 四、实验内容与实现

<!-- 
说明：本部分按照 exercises 文档中的练习顺序组织
每个功能模块或者练习中可能包含多项任务，请根据实际情况调整
-->

### 功能模块一：虚拟内存页操作

**负责人：** 2413497-姚嘉乐

#### 模块功能描述

**需要实现的函数**

```c
int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm);
void page_remove(pde_t *pgdir, uintptr_t la);
```

**所在文件** `code/kern/mm/pmm.c` 中的 `LAB4 function1`。

**功能说明**

两个函数都通过 `get_pte()` 找到 `la` 对应的末级页表项。插入时允许创建缺少的下级页表。末级项为空时给目标物理页增加一次引用，原来就指向同一页时只更新权限，原来指向另一页时先撤销旧映射。最后根据物理页号和本次传入的权限重新构造页表项，再刷新该地址的 TLB。重新构造页表项可以清除本次撤销的权限位。

删除时只查找已有的页表路径。找到有效映射后，`page_remove_pte()` 先记下旧物理页，再清空页表项并刷新 TLB，最后将旧页的引用计数减一，当页引用为0时将它归还给分配器。

```text
page_insert → get_pte(..., 1) → 必要时调用 page_remove_pte()
            → pte_create() → 写入页表项 → tlb_invalidate()
page_remove → get_pte(..., 0) → 末级项有效时调用 page_remove_pte()
```

#### Spec 与提示词设计

我先读 `get_pte()` 的实现，确认插入和删除时的查表方式，再对照 `check_pgdir()` 和评分中的 PMM 用例，按原映射为空、指向同一页或指向另一页写出各自的引用计数和权限要求。页表分配失败与重复删除也单独列出，便于检查失败时是否改动了数据页，以及删除是否多减了一次引用。

在提示词里，我用 `[PROMPT]` 限定修改位置，用 `[RELY]` 列出项目已有接口，用 `[GUARANTEE]` 写明两个函数签名，再把上述情形写入 `[SPECIFICATION]`。替换或删除映射时，要先撤销旧映射并刷新 TLB，才能回收引用归零的旧页。详细说明可以见prompt.md。

#### 最终提示词

以下为实际使用的完整最终提示词。

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

#### 实现迭代过程

使用这版提示词完成实现后，验证后确认实现正确，没有出现问题。

---

### 功能模块二：进程创建及初始化

**负责人：** 2410690-王睿瑜

#### 模块功能描述

**需要实现/修改的函数：**

修改文件为 `code/kern/process/proc.c`，对应框架中的 `LAB4 function2`。本模块实现以下函数。

```c
static struct proc_struct *alloc_proc(void);
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags);
static void copy_thread(struct proc_struct *proc, uintptr_t stack,
                        struct trapframe *tf);
static int copy_mm(uint32_t clone_flags, struct proc_struct *proc);
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf);
void proc_init(void);
```

**功能说明：**

本模块负责内核线程的创建与初始化，为调度器提供可运行的线程。系统启动时，`proc_init()` 将当前执行现场登记为 `idleproc`，让它复用启动栈、成为当前线程，并将它计入进程总数。随后通过 `kernel_thread()` 创建执行 `init_main()` 的 `initproc`。

创建新线程时，`do_fork()` 分配进程控制块（PCB）和内核栈，准备首次运行所需的 Trap 帧与切换上下文，再分配 PID，将线程登记到进程链表和 PID 哈希表，并置为就绪态。如果创建过程中发生错误，函数返回相应错误码，并释放已经分配的资源。

本模块调用内存管理模块的接口分配 PCB 和内核栈，创建的内核线程共享启动页表。调度模块选中新线程后，通过 `switch_to()` 恢复切换上下文，从 `forkret()` 进入 Trap 恢复流程，最终执行创建时指定的线程函数。

#### Spec 与提示词设计

模块二的规格按 PCB 初始化、首次运行现场、创建与登记、启动初始化的顺序编写。对照线程入口和 Trap 恢复汇编，确定函数地址、参数、栈指针和入口地址的保存位置，再根据资源取得的顺序写明各个失败点的清理要求，以及先登记、再唤醒的顺序。

实际提示词用 `[PROMPT]` 写明基线和修改范围，以 `[RELY]` 指向 C 结构体、汇编入口和已有辅助接口。`[GUARANTEE]` 列出六个函数的签名，`[SPECIFICATION]` 按函数写出调用前提、完成后的状态和分配失败时需要释放的资源。

设计依据见prompt.md中。

#### 最终提示词

以下是首轮在 VS Code 中发送给 Codex 的完整提示词。第二轮的修正提示词见下文迭代过程。

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

##### 第一次迭代

我在基于框架提交 `a211d8d` 创建的独立工作区中，将首轮完整提示词发送给 Codex。Codex 在 `code/kern/process/proc.c` 中引入 `riscv.h`，将标记改为 `//LAB4 function2 2410690`，并实现了 `alloc_proc()`、`kernel_thread()`、`copy_thread()`、`copy_mm()`、`do_fork()` 和 `proc_init()`。由于 Codex 无法解析该工作区的 `.git` 指针，我随后在 WSL 中核对了基线和改动范围。`git diff --check` 无输出，`proc.o` 强制重编译通过。

人工检查发现，首轮代码只在 `SSTATUS_SPP` 已为 1 时再次置位。如果读取到的 `SPP` 为 0，这段代码会保留原值，首次执行 `sret` 时会尝试返回 U 模式。新建内核线程应在 S 模式启动，因此需要无条件设置 `SPP`。首轮代码还将 `idleproc` 加入了 `proc_list`，我把这项处理也交给 Codex 对照框架复核。

为修正 `SPP` 并复核 `idleproc` 的登记方式，我第二次发送了以下提示词。

```text
请在当前试跑工作区做第二轮修正，只修改 code/kern/process/proc.c 的 function2 区域。

1. 修正 kernel_thread() 对 sstatus 的初始化。当前代码只有在 SSTATUS_SPP 已经置位时才再次置位。请基于 read_csr(sstatus) 无条件设置 SSTATUS_SPP 和 SSTATUS_SPIE，并清除 SSTATUS_SIE，确保首次 sret 返回 S 模式。
2. 对照 get_pid()、调度器和 code/tools/test_lab4.c，核对 proc_init() 是否应将 idleproc 插入 proc_list。说明源码依据，再决定是否修改。
3. 完成后列出实际改动。若无法读取 Git diff，请如实说明。编译和 QEMU 由我执行。
```

##### 第二次迭代

Codex 将 `kernel_thread()` 中的条件置位改为 `tf.status |= SSTATUS_SPP;`，同时保留 `SPIE` 置位和 `SIE` 清零。复核 `get_pid()`、调度器和 `test_lab4.c` 后，Codex 保留了 `idleproc` 在 `proc_list` 中的登记，PID 哈希表中仍未登记 `idleproc`。

修正后，我在 WSL 中再次执行 `git diff --check` 和 `make -B -C code obj/kern/process/proc.o`。前者无输出，后者强制重编译通过。第二轮实现用于正式提交 `40e9808`。本轮先完成单文件编译，整机结果在小组合并后验证。

**最终结果**

经过 2 次 Codex 迭代，完成了 `alloc_proc()`、`kernel_thread()`、`copy_thread()`、`copy_mm()`、`do_fork()` 和 `proc_init()`。第二轮将 `SSTATUS_SPP` 改为无条件置位，修正版用于正式提交 `40e9808`。

在 WSL 中执行 `git diff --check`，未发现补丁格式问题。执行 `make -B -C code obj/kern/process/proc.o`，强制重编译通过。小组合并后的 QEMU 与课程评分结果见第五节。

**合并验证**

原样合入 `40e9808` 时，`idleproc` 位于 `proc_list` 中，调度器扫描到它会提前选中，`make grade` 的调度项未通过，总分为 20/30。这只是一个小的约定问题，我们在 `proc_init()` 中删去 `list_add(&proc_list, &idleproc->list_link);`，由原有调度逻辑在没有普通就绪线程时回退到 `idleproc`。改后复测时，`make qemu` 成功运行 `initproc`，`make grade` 三项均通过，总分为 30/30。

### 功能模块三：进程调度

**负责人：** 2411333-吴卓彦

#### 模块功能描述

| 函数 | 实现与边界条件 |
| --- | --- |
| `cpu_idle()` | 在 `proc.c` 中循环检查 `current->need_resched`，标记置位才调用调度器。 |
| `schedule()` | 在 `sched.c` 中保存本地中断状态，清除当前线程的调度标记。当前是 idle 时从链表头之后开始，否则从当前线程之后开始，绕回时跳过哨兵和非就绪节点。找不到可运行的普通线程时选 `idleproc`。选中者的 `runs` 加一，目标与当前不同时才切换。 |
| `proc_run()` | 在 `proc.c` 中更新 `current`，将目标页表根的物理页号与 Sv39 模式位组合后写入 `satp`，刷新 TLB，再调用 `switch_to()`。切出的线程以后恢复执行时，从原调用位置继续，并恢复进入临界区前保存的中断状态。 |
| `switch_to()` | 在 `switch.S` 中按 `struct context` 的顺序保存、恢复 `ra`、`sp` 和 `s0`～`s11`。恢复 `ra` 后返回，新线程从预设的 `forkret` 开始，已运行过的线程回到上次切出的位置。 |

首次运行还需要此前迁移的 Trap 恢复代码。`forkrets` 把 `sp` 指向新线程的 trapframe，`RESTORE_ALL` 恢复寄存器和 CSR，最后由 `sret` 跳到 `kernel_thread_entry`。

Lab 3 的时钟处理中保留重设下一次事件、累加 `ticks` 和每 100 ticks 输出，移除了当时我自己加入的**累计 300 ticks 主动关机**的规则。

#### Spec 与提示词设计
详细的设计依据及思路见prompt.md。

#### 最终提示词

我先把调度的输入、修改边界、链表绕回和 idle 回退等要求写成 Spec，再按 `[PROMPT]`、`[RELY]`、`[GUARANTEE]`、`[SPECIFICATION]` 组织独立提示词。四个函数分别列出前置条件和后置条件。

```text
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

#### 实现迭代过程

1. **前置迁移。** 将 Lab 3 的中断与异常处理、Trap 入口和现场恢复迁入 Lab 4，保留时钟重设、计数和周期输出，去掉固定关机规则。
2. **首次生成。** AI 按 Spec 一轮完成四个调度函数。并且通过静态审查确认调度逻辑和上下文布局符合 Spec。

**最终结果**

- ✅ 完成 `cpu_idle()`、`schedule()`、`proc_run()` 和 `switch_to()`。
- ✅ 补丁格式检查通过，链表选择规则和 14 个上下文槽位已核对。

**关键改进点总结**

1. 在 `[RELY]` 中明确 `struct context` 的寄存器顺序和新线程的首跑路径，便于对照 C 与汇编接口。
2. 在 `[SPECIFICATION]` 中明确链表绕回、idle 回退和选中当前线程时的行为，并说明旧线程恢复执行后何时还原中断状态。

![AI 按 Spec 实现调度模块的记录](./images/lab4-schedule-ai-generation.png)

---

### Challenge 1：本地中断的保存与恢复

**负责人：** 2411333-吴卓彦

#### 题目

说明 `local_intr_save(intr_flag); ... local_intr_restore(intr_flag);` 如何实现开关中断。

#### 解答

这对宏在进入临界区时关闭当前 hart 的 S 态全局中断，并在退出时恢复进入前的状态。`local_intr_save(intr_flag)` 读取 `sstatus.SIE`，将原先的开关状态存入 `intr_flag`。如果中断原本开启，就调用 `intr_disable()` 清除 `SIE`。临界区执行完毕后，`local_intr_restore(intr_flag)` 读取这个保存值。原本开启就调用 `intr_enable()` 重新开启，原本关闭就保持关闭。下图展示了这两种情况。

```mermaid
flowchart TD
    A["local_intr_save(intr_flag)"] --> B{"sstatus.SIE 原来开启？"}
    B -->|"是"| C["保存 true，清除 SIE"]
    B -->|"否"| D["保存 false，保持 SIE 关闭"]
    C --> E["执行临界区，此时 SIE = 0"]
    D --> E
    E --> F["local_intr_restore(intr_flag)"]
    F --> G{"intr_flag 为 true？"}
    G -->|"是"| H["调用 intr_enable()，重新开启中断"]
    G -->|"否"| I["保持中断关闭"]
```

以 `schedule()` 调用 `proc_run()` 为例，假设进入调度前中断开启。

1. `schedule()` 保存 `true` 并关中断，随后选定下一个线程。
2. `proc_run()` 进入内层临界区时中断已关闭，因此保存 `false`。旧线程被再次调度后，从 `switch_to()` 返回，内层恢复操作保持中断关闭。
3. 旧线程继续退出 `schedule()` 的外层临界区，外层按保存的 `true` 重新开启中断。

这样，当前线程、页表和寄存器现场的交接可以在本地关中断期间完成。该机制保护当前 hart 的临界区。多核共享数据还需要相应的同步机制。

### Challenge 2：深入理解不同分页模式的工作原理（思考题）

**负责人：** 2413497-姚嘉乐

#### 题目

1. 结合 Sv32、Sv39、Sv48 的异同，解释 `get_pte()` 中的两段代码为什么相像。
2. `get_pte()` 把页表项查找和页表页分配放在同一个函数里。这种写法是否合适，有没有必要把两个功能拆开？

#### 解答

**1. 两段代码的作用**

Lab4 使用 Sv39。`get_pte()` 的第一段用 `PDX1(la)` 选出根页表项，从该项找到中间页表。第二段用 `PDX0(la)` 选出中间页表项，再从该项找到末级页表。两段跨越的是不同层级，但执行的是同一种操作：检查当前项的 `PTE_V` 位。如果下一级页表缺失且允许创建，就分配并清零一页，再把新页的物理页号写入当前项。到达末级页表后，`get_pte()` 用 `PTX(la)` 选中目标项并返回它的地址。

三种模式都把虚拟地址拆成各级索引和页内偏移。查找 4 KiB 页时，最低 12 位都是页内偏移，每级索引用来选择当前页表中的一项。

| 分页模式 | 各级索引位数 | 页表层数 | 查找 4 KiB 页时向下走的次数 |
| --- | --- | ---: | ---: |
| Sv32 | 10 + 10 | 2 | 1 |
| Sv39 | 9 + 9 + 9 | 3 | 2 |
| Sv48 | 9 + 9 + 9 + 9 | 4 | 3 |

每多一级页表，就要再执行一次查找下级页表的操作。按 Lab4 的写法，Sv32 只需写一段这样的检查，Sv39 需要写两段，Sv48 则需要写三段。两段代码相像，正是因为它们重复了同一个逐级查找步骤。

**2. 查找与分配是否需要拆开**

我认为本实验保留现有写法即可。查找和按需分配走同一条页表路径，`create` 决定缺少下级页表时如何处理。`page_remove()` 传入 `0`，缺表时直接返回。`page_insert()` 传入 `1`，遇到缺表才尝试分配。两个调用方因此可以共用一套遍历代码。

如果后续需要区分缺表和内存不足，或要求分配失败时撤销本次已挂接的页表等操作，可以提供「只查找」和「查找或创建」两个接口，内部仍共用遍历代码。

---

## 五、测试与验证

合并三个模块并修正 `idleproc` 的登记方式后，我们在 WSL 的 `code` 目录执行了 `make qemu` 和 `make grade`。普通启动完成了内存自检，`initproc` 成功运行。课程评分的三项测试全部通过，总分为 **30/30**。 下图沿用首次整合的截图，修正后的复测结果一致。

### 普通启动与内核线程运行

![执行 make qemu 后的编译输出](./images/lab4-qemu-build.png)

![链接器清理未使用的段并生成内核镜像](./images/lab4-qemu-linker.png)

前两张图记录了内核的编译与链接过程。编译时，`kmalloc.c` 把整数形式的地址传给了接收 `void *` 的 `kva2page()`，因此出现类型转换警告。我们查阅了对应源码和构建选项，确认该警告未中断本次编译。`removing unused section` 是链接器按构建选项移除未使用段时打印的信息。链接完成后，`objcopy` 生成了内核镜像 `bin/ucore.img`。

![OpenSBI 启动与 DTB 内存探测](./images/lab4-qemu-boot.png)

![内存自检通过及 initproc 的运行输出](./images/lab4-qemu-self-checks.png)

启动后，内核从设备树中读到了 128 MiB 物理内存。随后，物理页分配自检 `check_alloc_page()`、页表自检 `check_pgdir()` 和启动页表自检 `check_boot_pgdir()` 均通过。`initproc` 以 PID 1 运行，打印了 `Hello world!!` 和结束语，验证了内核线程的创建和首次切换。

### 课程评分

![make grade 注入测试并编译时的类型警告](./images/lab4-grade-warnings.png)

`make grade` 在临时副本中注入课程测试。除上述地址转换警告外，评分代码把返回 `void` 的测试函数传给了要求返回 `int` 的 `kernel_thread()`，因此出现函数指针类型警告。这些警告来自框架和评分代码中的类型不匹配，本次编译与评分仍顺利完成。

![PMM、进程创建和调度三项测试均通过，总分 30/30](./images/lab4-grade-result.png)

评分输出中，`check PMM`、`check Process Create` 和 `check Schedule` 均为 `OK`，每项满分 10 分。三项结果分别验证了页映射与引用计数、内核线程创建，以及两个测试线程都能得到调度并实际运行。

---

## 六、实验总结与收获

### 对操作系统的理解

#### 实验知识点与 OS 原理的对应

1. **PCB 与状态转换。** `proc_struct` 保存 PID、状态、内核栈和上下文等信息。新 PCB 从 `PROC_UNINIT` 开始，由 `wakeup_proc()` 置为 `PROC_RUNNABLE`，后续由调度器选择。
2. **线程资源共享。** 两个内核线程可以共享内核地址空间，同时保有各自的 PCB 和栈。`idleproc` 复用启动栈，`initproc` 使用单独分配的内核栈。
3. **切换上下文与 Trap 返回。** `context` 用于恢复内核切换所需的寄存器，`trapframe` 用于恢复 Trap 返回现场。首次运行通过预先构造这两份状态，把新线程送到指定入口。
4. **进程标识和并发保护。** PID 分配和链表、哈希表登记在同一段本地关中断区间内完成，防止本 CPU 的中断路径在登记过程中观察到不一致状态。
5. **调度策略与执行现场。** `schedule()` 按就绪状态和链表顺序选线程，`proc_run()` 和 `switch_to()` 完成执行权交接。实验中的测试线程主动让出 CPU，可以验证多次切出和恢复。
6. **本地中断与原子性。** 调度过程中保存并关闭当前 hart 的中断，使当前线程、地址翻译和寄存器现场在交接过程中保持一致。
7. **多级页表与物理页共享。** 分页用页表把虚拟页号转换为物理页号，页内偏移保持原值。实验中，`get_pte()` 按 Sv39 的三级索引找到末级页表项，`page_insert()` 可以让两个虚拟页指向同一个物理页。删除映射时，只有最后一份引用被撤销后，才能将数据页归还给分配器。
8. **页表更新与 TLB 同步。** 页表保存映射和访问权限，TLB 缓存近期的地址翻译。实验在替换或删除页表项后执行 `sfence.vma`，使处理器后续访问按更新后的映射处理。撤销映射时先清除页表项并同步 TLB，再回收旧页，避免处理器继续通过旧翻译访问已归还的物理页。

#### OS 原理中重要但没有对应上的知识点

1. **更复杂的调度机制。** 实验用链表扫描和主动重调度展示线程切换。完整的时间片抢占、优先级调度、多核运行队列和负载均衡还需要额外机制。
2. **按需分页和页面置换。** 缺页时如何按需调入页面、内存不足时如何选择并换出页面，这些更复杂的虚拟内存机制在本实验中还未涉及。
3. **用户进程的地址空间隔离与系统调用。** 本实验中的线程共享启动页表，`copy_mm()` 仅保留接口。用户进程需要独立的用户地址空间，还要通过系统调用完成从用户态进入内核态、处理请求和返回用户态的过程。
4. **进程退出与资源回收。** 本实验的 `do_exit()` 通过 `panic()` 结束演示。完整的退出流程还需要记录退出状态、通知等待的父进程，并在合适的时机回收内核栈和 PCB。

### AI 协作开发的经验

给AI写提示词时，先把几种容易混淆的情况分清楚，既能够提供AI生成代码准确率，又便于我们根据这些情况来验证AI生成代码的正确性。比如，同一个虚拟地址再次指向同一个物理页，只需要更新权限。如果换成了另一页，就要减少旧页的引用，等计数归零后再回收。把这两种情况分别写进提示词后，可以通过具体的情况处理来判断代码实现是否正确。

线程创建中的一些细节要结合源码确认。比如，线程函数和参数到底该放在哪个寄存器里，看 `kernel_thread_entry` 里的 `move a0, s1` 和 `jalr s0` 就清楚了。

---
