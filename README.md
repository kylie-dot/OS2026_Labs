# OS2026 Lab2：物理内存管理与页表

本实验在 Lab1 最小内核的基础上实现物理内存初始化、连续物理页分配和 SLUB 对象分配，并分析 Sv39 分页及 QEMU 的地址翻译过程。实验分支为 `lab2`，吴卓彦的个人开发分支为 `lab2-wzy`。

## 小组成员

| 学号 | 姓名 | 负责内容 |
|------|------|----------|
| 2411333 | 吴卓彦 | `page_init()`、Buddy System、构建系统调查、报告整合 |
| 2413497 | 姚嘉乐 |First-Fit 分析、Best-Fit、SLUB 分配器 |
| 2410690 | 王睿瑜 |Sv39 页表与启动过程、GDB 调试 QEMU、可用物理内存探测思考题 |

各成员负责自己模块的报告、提示词和测试记录，最后统一整合。

## 仓库约定

每个实验对应一个 `labx` 分支（`lab1`、`lab2`、…），分支根目录包含：

- `code/` — 该实验实现后的源代码
- `report/` — 实验交付物：
  - `report.md` — 实验报告
  - `prompt.md` — 本实验全部提示词汇总
  - `images/` — 报告中引用的测试截图

主要代码位置：

| 路径 | 用途 |
|------|------|
| `code/kern/init/` | 保存启动参数、启用 Sv39 分页、执行内核初始化 |
| `code/kern/driver/dtb.c` | 解析设备树，获取物理内存范围 |
| `code/kern/mm/` | 页描述符、地址转换、物理内存管理与分配算法 |
| `code/libs/` | 链表、字符串、格式化输出及 SBI 接口 |
| `code/Makefile`、`code/tools/` | 构建、链接、调试及评分工具 |

## 实验进度

截至 **2026 年 10 月 23 日**，已有实现与验证如下：

| 任务 | 状态 |
|------|------|
| `page_init()` | 已实现；默认 QEMU 环境初始化与课程评分检查通过 |
| First-Fit 分析与 Best-Fit 实现 | 已完成四函数分析及改进空间说明；Best-Fit 已实现并作为默认管理器，固定测试通过 |
| Buddy System 与测试 | 已实现，支持独立编译选择；初始化、拆分合并、碎片化和失败状态四组测试通过 |
| SLUB 分配器与测试（Challenge） | 已实现页级与对象级两层分配；六组测试在 Best-Fit 和 Buddy 两种底层模式均通过 |
| 页表分析与 QEMU 调试 | 已记录启动分页、页表查询、软件 TLB 填充与命中、Bare 模式对比及调试截图 |
| 可用物理内存探测思考题 | 已在报告说明 DTB、UEFI 内存映射和 BIOS E820 等方法 |
| 构建系统调查 | 已分析链接脚本、Makefile 相对 Lab1 的变化、测试注入及评分调用链 |
| 报告与提示词 | 已补充模块说明、完整提示词、真实迭代过程、测试证据与原理对照；`prompt.md` 保留提示词汇总 |

默认模式 `make grade` 的 `check page init` 和 `check best fit` 均为 OK，结果为 **20/20**。该评分覆盖课程的两项检查；Buddy 四组测试与 SLUB 六组测试分别保留运行证据，Challenge 加分以助教验收为准。

## 运行与验证

实验环境为 WSL Ubuntu，使用 SiFive RISC-V GCC 10.2.0 工具链和 QEMU 4.1.1。先将 `riscv64-unknown-elf-` 工具链加入 `PATH`，再在 WSL 中进入本分支的 `code/` 目录：

```bash
make          # 编译、链接并生成内核镜像
make qemu     # 启动内核；退出时按 Ctrl+A，松开后按 X
make grade    # 在临时副本中注入测试并评分
```

默认使用 `best_fit_pmm_manager`。运行 Buddy 模式时，先清理旧构建产物，再启用编译开关：

```bash
make clean
make BUDDY=1 qemu
```

应看到 `memory management: buddy_pmm_manager`、四组 `[BUDDY] ...: PASS`、`[BUDDY] independent tests passed!`，以及后续 `[SLUB] all tests passed!`。

退出 QEMU 后，恢复默认 Best-Fit 模式同样先清理：

```bash
make clean
make qemu
# 退出 QEMU 后再执行：
make grade
```

GDB 调试时，在两个 WSL 终端的 `code/` 目录分别执行 `make debug` 和 `make gdb`。

`make test` 使用带 `ucore_test` 宏的编译模式并启动 QEMU；`make grade` 才会调用 `inject.py` 在临时副本中加入课程测试，再由 `grade.sh` 检查输出并计分。切换编译模式前先执行 `make clean`。

## 报告与验证证据

- [实验报告](report/report.md)：功能说明、提示词、实现迭代、QEMU 调试过程及知识点总结。
- [提示词汇总](report/prompt.md)：各编码任务的 Spec、实现/测试提示词及补充修改记录。
- [Buddy 独立测试与 SLUB 运行结果](report/images/buddy-v3-independent-tests-pass.png)。
- [默认 Best-Fit 模式回归结果](report/images/buddy-regression-bestfit-slub-pass.png)。
- [课程评分 20/20](report/images/buddy-regression-grade-20.png)。

## 提交要求

- 保留代码中的 `lab2` 任务注释，将 `YOUR CODE` 替换为实现者学号；不修改框架规定保留的测试函数。
- 编码任务在 `report/prompt.md` 中记录 Spec、最终提示词和真实迭代过程；Buddy 与 SLUB 分别记录实现提示词和测试提示词。
- 各模块报告包含功能与核心函数分析、测试证据、AI 生成代码的问题及修正，以及与 OS 原理知识点的对应关系；全组汇总原理中尚未在实验体现的知识点。
- 测试截图放入 `report/images/`，在 `report/report.md` 中使用相对路径引用；个人开发分支的改动最终合入 `lab2` 分支。
