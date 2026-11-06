# OS2026 Lab3：中断与异常处理

本分支为 `Lab3`。实验在 Lab 2 内存管理与分页代码的基础上，完成 RISC-V Trap 入口、上下文恢复、中断与异常处理，并通过断点、非法指令和时钟中断验证完整流程。

实验手册：[Lab 3](http://oslab.mobisys.top/lab2026/_book/lab3/lab3.html) · [练习](http://oslab.mobisys.top/lab2026/_book/lab3/lab3_2_1_exercise.html) · [报告要求](http://oslab.mobisys.top/lab2026/_book/lab3/lab3_4_lab_requirement.html)

## 小组分工

| 任务 | 姓名 | 学号 | 负责范围 |
|---|---|---|---|
| A：Trap 汇编入口与返回 | 姚嘉乐 | 2413497 | `code/kern/trap/trapentry.S` 中的 `RESTORE_ALL`、`__alltraps`；Trap frame 传参、寄存器和 CSR 恢复 |
| B：中断分类与时钟处理 | 吴卓彦 | 2411333 | `code/kern/trap/trap.c` 中的 `interrupt_handler()`；时钟重设、`ticks` 统计、其他中断分类与回退输出 |
| C：异常处理与复核 | 王睿瑜 | 2410690 | `code/kern/trap/trap.c` 中的 `exception_handler()`；断点和非法指令的 `epc` 更新、异常回退与跨模块复核 |

三人分别记录自己任务的 Spec、Prompt、实际 bug 与修复、测试证据和报告章节。B、C 都会编辑 `trap.c`，各自只修改负责的函数，合并时按函数检查差异。

## 代码结构

| 路径 | 用途 |
|---|---|
| `code/kern/trap/trapentry.S` | Trap 入口、现场保存与恢复、调用 C 层 `trap()` |
| `code/kern/trap/trap.c`、`trap.h` | Trap frame 定义、原因分派、中断与异常处理 |
| `code/kern/driver/clock.c`、`intr.c` | OpenSBI 时钟事件与 S 模式中断使能 |
| `code/kern/sync/sync.h` | 临界区关中断和恢复 |
| `code/kern/mm/` | 延续 Lab 2 的物理内存管理与页表代码 |
| `code/tools/grade.sh`、`inject.py` | Lab 3 本地测试注入和评分 |
| `report/` | 报告、提示词和图片；目前仍是 Lab 2 内容，待更新 |

## 当前状态

- `code/` 已包含下载源码 `E:\大三上\OS\labs\lab3\lab3_new` 的全部文件；同名文件内容逐项一致。
- 目录还保留六个 Lab 2 的 Buddy/SLUB 源码和测试文件，属于既有成果。
- `trapentry.S` 的两处 `lab3 function1 YOUR CODE` 和 `trap.c` 的 `lab3 function2 YOUR CODE` 尚待实现，当前未验证 Lab 3 编译或评分。
- 代码树目前有未提交的 Lab 3 框架改动。提交前保留 `lab3` 标记，并将相关 `YOUR CODE` 替换为实现者学号。

## 构建与验证

在配置好 RISC-V 交叉编译工具链和 QEMU 的 WSL/Linux 环境中，进入 `code/` 执行：

```bash
make          # 编译内核
make qemu     # 启动 QEMU
make grade    # 在临时副本中注入测试并评分
```

当前评分脚本有三组：Trap 入口和断点返回 35 分、非法指令返回 25 分、持续时钟中断 40 分。实现完成后还需核对手册要求与真实运行结果。

## 报告与提交

按手册用 Markdown 说明整体 Trap 执行流、各模块核心函数、每个练习的 Spec 和 Prompt、AI 代码的真实问题与修复，以及实验知识点与 OS 原理的关系。报告、提示词和截图放在 `report/`；最后核对注释中的学号，提交 `Lab3` 分支。
