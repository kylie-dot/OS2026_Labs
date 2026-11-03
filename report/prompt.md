# Lab3 提示词汇总（prompt.md）

> 沿用 Lab2 的记录方式：每项任务按 **Spec（设计依据）→ 最终提示词 → 实现迭代过程** 组织，保留实际使用的提示词和修正过程。
> 各成员填写本人负责的部分。AI 按规格直接修改代码，编译、QEMU 运行、评分和截图由本人完成；静态审查与实际运行结果分别如实记录。

---

## 功能模块一：Trap 上下文恢复与 Trap 入口功能

本模块由其他成员负责，Spec、实际提示词和迭代结果待负责人补充。

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

本任务由其他成员负责，Spec、实际提示词和迭代结果待负责人补充。
