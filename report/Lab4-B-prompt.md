# Lab4 提示词（B 模块）

## 功能模块二：进程创建及初始化

**负责人**　王睿瑜 2410690（`code/kern/process/proc.c`）

### 一、Spec（设计依据）

**1. 输入与调用条件**

- 以小组仓库 `kylie-dot/OS2026_Labs` 的 Lab4 框架提交 `a211d8d` 为基线。目标文件中的 `LAB4 function2 YOUR CODE` 是 B 模块的实现位置。
- `proc_init()` 在内核启动阶段调用，依赖内存管理已初始化、启动页表和启动栈可用。普通线程由 `kernel_thread()` 提供初始 Trap 帧，再交给 `do_fork()`。
- 以本仓库的头文件和汇编为准。线程入口使用 `s0` 保存目标函数，`s1` 保存参数。`forkrets` 将传入的 Trap 帧地址装入 `sp`，再走既有 Trap 恢复路径。

**2. 输出与处理过程**

- 初始化 PCB、创建 `idleproc` 和 `initproc`，为新线程准备独立内核栈、Trap 帧与切换上下文。
- 创建成功时分配唯一 PID，登记到进程链表和 PID 哈希表，更新进程计数，再将线程设为就绪。失败时返回负错误码并释放此前取得的资源。
- 内核线程共享启动页表，当前阶段保留 `copy_mm()` 接口。线程创建与调度通过 PCB 中的状态、栈、Trap 帧和切换上下文衔接。

**3. 修改范围与禁止项**

仅编辑 `code/kern/process/proc.c` 的 B 模块，以及编译所需的头文件引用和外部符号声明。保留 C 模块的 `LAB4 function3 YOUR CODE`，沿用现有数据结构、辅助函数与公共接口。

### 二、最终提示词

```text
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
```

### 三、Codex 迭代与验证

#### 第一轮：按完整提示词生成 B 模块

**发送的提示词**　使用第二节的完整提示词。在 VS Code 中打开基于框架提交 `a211d8d` 的独立 worktree，新建 Codex 会话并一次性发送。

**实现与检查**　Codex 在 `code/kern/process/proc.c` 中实现了六个函数，更新了学号标记，并引入 `riscv.h`。Codex 无法解析 worktree 的 `.git` 指针，依据源码中的空框架继续实现。随后在 WSL 核对基线和改动范围。已跟踪文件中只有 `proc.c` 发生变更，相对框架新增 101 行、删除 1 行。另有未跟踪的 `.vscode/`。

**发现的问题**　`git diff --check` 无输出，`make -B -C code obj/kern/process/proc.o` 强制重编译通过。人工审查发现，`kernel_thread()` 只在 `SSTATUS_SPP` 已为 1 时再次置位。若读取值为 0，首次 `sret` 就无法保证返回 S 模式。

#### 第二轮：修正 SPP 初始化并复验

**发送的修正提示词**

```text
请在当前试跑工作区做第二轮修正，只修改 code/kern/process/proc.c 的 function2 区域。

1. 修正 kernel_thread() 对 sstatus 的初始化。当前代码只有在 SSTATUS_SPP 已经置位时才再次置位。请基于 read_csr(sstatus) 无条件设置 SSTATUS_SPP 和 SSTATUS_SPIE，并清除 SSTATUS_SIE，确保首次 sret 返回 S 模式。
2. 对照 get_pid()、调度器和 code/tools/test_lab4.c，核对 proc_init() 是否应将 idleproc 插入 proc_list。说明源码依据，再决定是否修改。
3. 完成后列出实际改动。若无法读取 Git diff，请如实说明。编译和 QEMU 由我执行。
```

**修正结果**　Codex 将条件置位改为 `tf.status |= SSTATUS_SPP;`，保留 `SPIE` 置位和 `SIE` 清零。复核后沿用 `idleproc` 加入 `proc_list` 的实现，PID 哈希表中未登记 `idleproc`。

**复验与提交**　在 WSL 中重新执行 `git diff --check` 和 `make -B -C code obj/kern/process/proc.o`，均通过。第二轮生成的实现用于正式提交 `40e9808`。整机构建、QEMU 首次调度、失败清理路径和实验评分仍待验证。
