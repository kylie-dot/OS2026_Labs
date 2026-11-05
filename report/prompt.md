# Lab3 提示词汇总（prompt.md）

> 沿用 Lab2 的记录方式：每项任务按 **Spec（设计依据）→ 最终提示词 → 实现迭代过程** 组织，保留实际使用的提示词和修正过程。
> 各成员填写本人负责的部分。AI 按规格直接修改代码，编译、QEMU 运行、评分和截图由本人完成；静态审查与实际运行结果分别如实记录。

---

## 功能模块一：Trap 上下文恢复与 Trap 入口功能

**负责人：** 姚嘉乐 2413497（code/kern/trap/trapentry.S）

### 一、Spec（设计依据）

**1. 输入与调用条件**

- 采用框架已有的 `SAVE_ALL` 和 `struct trapframe` 布局；保存完成后，当前 `sp` 指向 Trap frame，`gpr.sp` 保存 Trap 前的原始栈指针。
- C 语言接口为 `void trap(struct trapframe *tf)`，按照 RISC-V 调用约定使用 `a0` 传递第一个参数。

**2. 输出与恢复要求**

- `__alltraps` 将已保存的 Trap frame 传给 `trap()`，返回后进入已有的 `__trapret`。
- `RESTORE_ALL` 从对应槽位恢复通用寄存器、`sstatus` 和 `sepc`；使用处理函数更新后的 EPC，并在完成其余读取后恢复原始 `sp`。
- 保留已有的 `sret` 返回流程，不改变保存的返回特权级。

**3. 修改范围**

仅补全两处待实现代码，不修改 `SAVE_ALL`、结构体布局、现有接口和其他文件。

### 二、最终提示词

以下为实际使用的完整最终提示词。

```text
[PROMPT]
任务：
完成 Trap 上下文恢复与 Trap 入口功能。
目标文件为 code/kern/trap/trapentry.S。

操作要求：
只补全 RESTORE_ALL 和 __alltraps 中的两处待实现代码。
不得修改 SAVE_ALL、Trap frame 布局、现有接口及其他文件。
保持代码可读，在关键步骤添加必要注释。

输出要求：
依据下面的依赖和规格，在项目中修改实际文件。
将两处标记改为“#lab3 function1 2413497”。

[RELY]
先阅读：
code/kern/trap/trapentry.S
code/kern/trap/trap.h
code/kern/trap/trap.c
code/libs/riscv.h

Trap frame 的结构为：

struct trapframe {
    struct pushregs gpr;
    uintptr_t status;
    uintptr_t epc;
    uintptr_t badvaddr;
    uintptr_t cause;
};

已有 SAVE_ALL 在当前内核栈上建立 Trap frame。
执行完成后，sp 指向该 Trap frame；
gpr.sp 保存的是分配 Trap frame 之前的原始 sp。

C 语言处理接口为：
void trap(struct trapframe *tf);

RISC-V 调用约定使用 a0 传递第一个参数。
trap() 可能修改 Trap frame 中的 epc。

[GUARANTEE]

1. 完成已有汇编宏 RESTORE_ALL。
2. 完成 __alltraps 中 SAVE_ALL 之后的调用衔接代码。

[SPECIFICATION]
实现单元：__alltraps

Pre-Condition：
处理器已经进入 S 模式 Trap 入口，相关 CSR 已由硬件设置。
当前 sp 指向有效且满足调用约定对齐要求的内核栈。

Post-Condition：
建立完整的 Trap frame，将其交给 trap() 处理；
trap() 返回后，进入现有的 __trapret 恢复流程。

实现单元：RESTORE_ALL

Pre-Condition：
trap() 已返回，当前 sp 指向完整且布局正确的 Trap frame。
其中的 epc 等状态可能已经被处理函数更新。

Post-Condition：
根据当前 Trap frame 恢复通用寄存器、sstatus 和 sepc，
恢复原始 sp，使后续已有的 sret 能够正确返回。
```

### 三、实现与验证过程

本模块使用一版最终提示词完成代码生成。生成后进行了相应测试验证，未发现需要修正的实现问题。

| 阶段 | 实际操作 | 结果 |
|------|----------|------|
| 首次生成 | 补全 `RESTORE_ALL`，在 `__alltraps` 中通过 `a0` 传递现场地址并调用 `trap()` | 两处标记均填写学号 2413497，已有保存与返回流程保持不变 |
| 评分验证 | 执行 `make grade` | 上下文与断点返回 35/35、非法指令处理 25/25、持续时钟中断 40/40，总分 100/100 |
| 普通启动 | 编译普通版本并运行 QEMU | 内存分配自检通过，连续输出三次 `100 ticks` 后正常关机 |

**关键体会：** 提示词需要明确当前 `sp` 与原始 `sp` 的区别，以及处理函数可能修改 EPC。恢复现场应以处理后的 Trap frame 为准。

---

## 功能模块二：中断与异常处理功能

### 中断处理：interrupt_handler()（吴卓彦 2411333，code/kern/trap/trap.c）

### 一、Spec（设计依据）

**1. 输入与调用条件**

- 函数签名为 `void interrupt_handler(struct trapframe *tf)`，输入为已保存现场的 Trap frame。
- `tf->cause` 保存 `scause` 的值。RV64 最高位是中断标志，其余 63 位为原因编号。
- `trap_dispatch()` 已按最高位区分中断与异常；本函数进一步区分具体中断。
- 原因编号提取应兼容完整的 `scause` 值和测试直接传入的不带最高位的编号。
- 使用框架现有的 `IRQ_*`、`ticks`、`TICK_NUM`、`clock_set_next_event()`、`print_ticks()`、`print_trapframe()` 和 SBI 接口。

**2. 输出与处理过程**

1. 原因编号为 `IRQ_S_TIMER`（5）时，先调用 `clock_set_next_event()`，再执行 `ticks++`。
2. `ticks` 为 `TICK_NUM` 的正整数倍时，调用已有的 `print_ticks()`。框架定义 `TICK_NUM=100`。
3. 手册提到 `SHUTDOWN_AFTER`，但框架未定义数值；本组选定为 3，表示第 3 次统计输出后关机，即第 100、200 次中断后继续运行，第 300 次输出后调用 `sbi_shutdown()`。
4. 其他已知中断按 `libs/riscv.h` 中的编号区分并输出类型提示，不修改 `ticks`，不预约时钟事件。
5. 未知中断先输出原因编号，再调用 `print_trapframe(tf)`。输出格式符必须匹配项目中 `uintptr_t` 的实际类型。
6. 不修改 `tf->epc`。

**3. 修改范围与禁止项**

- 在 `lab3 function2 YOUR CODE` 标记处新增 `interrupt_handler()`，将标记改为 `lab3 function2 2411333`。
- 仅修改该函数以及必要的头文件和宏；允许引入 `sbi.h` 和定义 `SHUTDOWN_AFTER`。
- 不修改已有函数、Trap frame 结构、汇编入口或测试文件；异常处理由其他成员实现。
- AI 不运行编译、QEMU 或评分，不提交或推送 Git；由本人执行验证并反馈。


### 二、最终提示词

以下为首次实际发送给 AI 的完整提示词。后续两次修正提示词见下一节；其中关于输出格式的要求，经审查补充为核对项目实际 typedef 和格式化器取参方式。

```text
你是 RISC-V RV64 教学内核的开发助手。
请先阅读指定代码，再严格按照以下 Spec 直接修改工作区文件，
完成时钟及其他中断处理函数。

项目目录：E:\大三上\OS\OS2026_Labs\code

【先阅读】
kern/trap/trap.c、kern/trap/trap.h
kern/driver/clock.c、kern/driver/clock.h
libs/riscv.h、libs/sbi.h

【目标和修改范围】
在 kern/trap/trap.c 的“lab3 function2 YOUR CODE”标记处，
新增并实现：

void interrupt_handler(struct trapframe *tf)

将该标记中的 YOUR CODE 改为实现者学号 2411333，保留 lab3 注释。
只修改此函数以及编译所必需的头文件和宏。不要修改已有函数、
Trap frame 结构、汇编入口或测试文件；不要实现异常处理函数。

【Spec：原因识别】
tf->cause 是 scause 保存到 Trap frame 中的值。RV64 的最高位表示中断，其余位是原因编号。现有 trap_dispatch() 已经区分中断与异常。请在 interrupt_handler() 中正确取得原因编号；
不能直接将包含最高位的完整 tf->cause 与 IRQ_S_TIMER 比较。
提取方式也应能处理测试代码直接传入原因编号的情况。

【Spec：时钟中断】
原因编号为 IRQ_S_TIMER 时：
1. 先调用 clock_set_next_event() 预约下一次时钟事件；
2. 再让 ticks 加一；
3. 每当 ticks 为 TICK_NUM 的正整数倍时，调用已有的 print_ticks()；
4. 本组自行选定 SHUTDOWN_AFTER=3，表示第 3 次统计输出后调用 sbi_shutdown()。第 100、200 次中断后必须继续运行，第 300 次输出后才关机。

trap.c 已定义 TICK_NUM=100。不要改动 print_ticks()。
如需使用 sbi_shutdown()，可添加必要的 sbi.h 头文件。

【Spec：其他中断】
依据 libs/riscv.h 中已有的中断编号，对已知的非时钟中断
进行基本类型区分并输出对应提示。非时钟中断不修改 ticks，
也不预约时钟事件。

Supervisor 软件中断的输出必须包含：
Supervisor software interrupt

对未知中断，先输出原因编号，再调用 print_trapframe(tf)
输出完整 Trap frame。未知编号 15 的输出必须包含：
Unknown interrupt: cause 15

不要在中断处理函数中修改 tf->epc。

【交付】
直接修改指定代码文件，并说明修改了哪些位置、为什么这样实现。
不要在 WSL 编译，不要运行 QEMU 或评分脚本，也不要声称代码已通过测试。
不要自行提交或推送 Git。编译、运行和截图由我亲自完成；
我会把真实结果反馈给你，再根据结果迭代。
```

### 三、实现迭代过程

| 轮次 | 实际现象 / 审查发现 | 原因分析 | 修正与结果 |
|------|--------------------|----------|------------|
| v1：首次生成与静态审查 | AI 在 trap.c 添加 sbi.h、SHUTDOWN_AFTER=3 和 interrupt_handler()；未知原因编号使用 `%d` 输出 | 原因提取、时钟处理顺序、周期统计、第三次输出后关机和未知中断兜底符合约定；但 cause 为 uintptr_t，`%d` 按 int 取参，类型不匹配 | 发送限定范围的修正提示词，仅修正未知中断输出；未运行编译或测试 |
| v2：第一次格式修正 | AI 将 `%d` 改为 `%lu`，声称 uintptr_t 为 unsigned long | 项目 defs.h 实际将 uint64_t 定义为 unsigned long long，uintptr_t 又定义为 uint64_t；printfmt.c 对 `%lu` 按 unsigned long 取参，类型仍不匹配 | 再次要求核对完整 typedef 链与 printfmt.c；未运行验证 |
| v3：第二次格式修正 | AI 将输出改为 `%llu`；静态核对确认文件已修改 | printfmt.c 在两个 l 后按 unsigned long long 取参，与本项目 cause 的实际类型匹配 | 编号 15 的文字保持不变；此处格式问题已修正 |

**第二次实际发送的提示词：**

```text
请只修正 `interrupt_handler()` 中未知中断的 
`cause` 打印格式，使格式符与实参类型匹配，并保持编
号 15 的输出文字不变。不要修改异常处理或其他函数。完
成后说明实际改动；不要运行验证。
```

**第三次实际发送的提示词：**

```text
请核对本项目 `libs/defs.h` 中 `uintptr_t` 的完整
 typedef 链，以及 `libs/printfmt.c` 对 `%llu` 的处
 理。只修正 `trap.c` 未知中断输出这一行，使格式符与
  `cause` 的实际类型匹配；保持 `Unknown 
  interrupt: cause 15` 输出不变。不要修改其他代码，
  也不要运行验证。
```

**关键改进：** 首次修正只要求“类型匹配”，AI 仍按常见平台类型作出判断。再次修正明确要求读取项目的 typedef 链和格式化器，才将 `%lu` 修正为 `%llu`。

---

### 异常处理：exception_handler()

### 异常处理：exception_handler()（王睿瑜 2410690，code/kern/trap/trap.c）

### 一、Spec（设计依据）

**1. 输入与调用条件**

- 函数签名为 `void exception_handler(struct trapframe *tf)`，输入为已保存现场的 Trap frame。
- `tf->cause` 保存异常原因，`tf->epc` 保存异常指令地址。
- `trap_dispatch()` 已根据原因值的最高位区分中断与异常，本函数只负责进一步处理同步异常。
- 使用框架已有的 `CAUSE_ILLEGAL_INSTRUCTION`、`CAUSE_BREAKPOINT`、`cprintf()` 和 `print_trapframe()`，不重复定义异常原因编号。
- 本实验对需要跳过的异常指令采用以下前提：指令所在内存可读，且指令长度属于实验涉及的 16 位或 32 位范围。

**2. 输出与处理过程**

1. 原因为 `CAUSE_ILLEGAL_INSTRUCTION`（2）时，输出：

   ```text
   Illegal instruction at %p\n
   ```

2. 原因为 `CAUSE_BREAKPOINT`（3）时，输出：

   ```text
   Breakpoint at %p\n
   ```

3. 上述输出中的地址使用当前 `tf->epc`，参数转换为 `void *`，与 `%p` 对应。
4. 对这两类异常，读取异常指令的低 16 位。在本实验的 16/32 位指令范围内，低两位不为二进制 `11` 时，将 `tf->epc` 增加 2；否则增加 4。
5. 对其他异常，调用已有的 `print_trapframe(tf)`，不改变 EPC，也不读取对应的指令地址。
6. 不修改保存的 cause、status 和通用寄存器。
7. 函数返回后，由统一的 Trap 返回流程恢复现场，将更新后的 EPC 写回 `sepc`，再通过 `sret` 返回。

**3. 修改范围与禁止项**

- 在 `trap.c` 的 `lab3 function2` 对应位置新增 `exception_handler()`，保留 lab3 标记并填写学号 2410690。
- 不覆盖队友的 `interrupt_handler()`。
- 不修改其他已有函数、Trap frame 布局、汇编入口或函数签名。
- 不为未知异常增加未经要求的恢复逻辑。
- 不将跳过非法指令描述为通用异常恢复策略。
- 不将主机端函数测试等同于真实 RISC-V Trap、现场恢复和 `sret` 验证。

### 二、最终提示词

以下为根据本次实际协作要求整理的最终提示词，用于汇总实现约束，不作为另一次实际发送的逐字记录。原始交互以本次 AI 对话记录为准。

```text
你是 RISC-V RV64 教学内核的开发助手。
请先阅读指定代码，再按照以下 Spec 实现异常处理函数。

【先阅读】
code/kern/trap/trap.c
code/kern/trap/trap.h
code/libs/riscv.h
code/libs/defs.h

必要时查阅格式化输出实现和测试汇编，核对地址输出方式、
异常原因编号和指令编码，不要根据其他项目猜测当前接口。

【目标和修改范围】
在 code/kern/trap/trap.c 的 lab3 function2 对应位置，
新增并实现：

void exception_handler(struct trapframe *tf)

保留 lab3 注释，填写学号 2410690。
只实现异常处理函数，不覆盖队友的 interrupt_handler()，
不修改其他已有函数、汇编入口、Trap frame 布局和函数签名。

【Spec：异常分类与输出】
现有 trap_dispatch() 已经区分中断与异常。
请根据 tf->cause，使用框架已有的宏进行分类：

CAUSE_ILLEGAL_INSTRUCTION
CAUSE_BREAKPOINT

非法指令异常输出：
Illegal instruction at %p\n

断点异常输出：
Breakpoint at %p\n

地址取自更新前的 tf->epc。
将地址参数转换为 void *，与 %p 对应。

【Spec：更新返回位置】
对非法指令和断点异常，读取 tf->epc 指向的指令低 16 位。

在本实验的 16/32 位指令范围内：
低两位不为二进制 11 时，tf->epc 增加 2；
否则，tf->epc 增加 4。

不要把所有异常指令一律按 4 字节跳过。
使用半字读取，避免为判断压缩指令而直接读取完整 32 位数据。

【Spec：其他异常】
调用已有的 print_trapframe(tf) 输出现场。
保持 EPC 不变，不读取未知异常对应的指令地址。
不要修改保存的 cause、status 和通用寄存器。

【边界说明】
该实现依赖异常指令所在内存可读。
长度判断仅适用于本实验涉及的 16/32 位指令，
不代表完整的 RISC-V 变长指令解码。
跳过非法指令只是实验规定的处理策略，
不能视为真实操作系统处理所有非法指令的通用方法。

【验证要求】
通过交叉编译检查函数是否能够编译，
通过反汇编核对测试断点的实际长度，
通过函数级测试检查 EPC 推进和未知异常回退行为。

明确区分函数级测试与真实 Trap 返回验证。
没有实际运行的 QEMU 和评分测试不得写成通过。
```

### 三、实现与验证过程

本任务完成了一版异常处理实现，随后进行了编译检查、反汇编核对、函数级测试和代码集成。现有验证中未发现异常处理算法本身的实际错误，因此以下按实际阶段记录，不将验证过程虚构为多轮算法修正。

| 阶段 | 实际操作或发现 | 分析与结果 |
|------|----------------|------------|
| v1：首次实现与接口核对 | 根据异常原因宏完成分类，输出异常地址；使用指令低 16 位判断推进 2 或 4 字节；其他异常调用 print_trapframe() | 保持函数签名和结构体布局不变，不覆盖中断处理；设计时已考虑压缩指令 |
| 编译检查 | 在中断处理尚未接入时，临时提供 interrupt_handler() 声明，对实际 trap.c 进行 RISC-V 交叉编译 | 编译检查通过；不代表完整工程运行通过 |
| 反汇编核对 | 查看 test_lab3_context.S 的反汇编，断点机器码为 9002，位于偏移 0x6a，下一条指令位于 0x6c | 确认该断点占 2 字节，验证了不能统一推进 4 字节；这是设计验证，不是已发生的程序错误 |
| 主机端函数测试 | 从实际代码提取函数，测试 16/32 位断点、16/32 位非法指令和未知异常回退 | 五项测试通过，记录为 5/5 passed；同时检查 cause、status 和 a0 的代表值未被修改 |
| 与队友代码集成 | 合并公共 Lab3 分支，解决同名框架文件的 add/add 冲突，同时保留中断处理和异常处理 | 集成后的 trap.c 编译通过，合并提交为 330e2cd |
| 集成兼容修复 | 实际编译提示 unknown CSR sbadaddr，将汇编中的旧名称改为 stval | 对应汇编文件编译通过；属于框架与工具链兼容修复，不是异常处理算法修改 |
| 完整构建检查 | 构建在 best_fit_pmm.c 处因 free_list、nr_free 未定义而停止 | 当前工程缺少对应 Lab2 实现；本次只整合 Lab3，未补写内存管理代码，完整运行和评分验证尚未完成 |

**函数级测试记录：**

| 测试输入 | 预期行为 | 结果 |
|----------|----------|------|
| 16 位断点 | EPC 增加 2 | PASS |
| 32 位断点 | EPC 增加 4 | PASS |
| 16 位非法指令 | EPC 增加 2 | PASS |
| 32 位非法指令 | EPC 增加 4 | PASS |
| 未知异常，cause=20 | 调用回退函数，EPC 不变 | PASS |

测试源码：

```text
code/local-tests/exception_handler_host_test.c
```

运行记录：

```text
report/evidence/c_exception_host_test.txt
```

**验证边界：**

- 主机端测试使用回退函数的测试替身，只验证调用行为，没有验证真实 `print_trapframe()` 的控制台输出。
- 测试源码包含从生产代码提取的函数快照。生产函数修改后，需要重新提取并测试，不能用旧快照证明新代码正确。
- 测试未触发真实 RISC-V Trap，也未执行 CSR 恢复和 `sret`，因此不能替代 QEMU 和评分验证。
- 最近一次已反馈的检查中，Trap 上下文恢复与入口部分仍有待实现标记，完整异常返回需在该模块接入后继续验证。

**关键经验：**

1. 提示词需要明确指令长度判断，不能默认所有异常指令都占 4 字节。
2. 未知异常的现场输出属于诊断，不代表故障已经恢复。
3. 需要分别记录编译检查、函数级测试和完整内核运行，避免扩大测试结论。
4. 整合共享文件时应保留各成员的函数，不能通过整份覆盖解决冲突。
