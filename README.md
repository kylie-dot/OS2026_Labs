# OS2026 Lab2：物理内存管理与页表

本分支为 `lab2`，在 Lab1 最小内核的基础上学习 Sv39 分页、物理内存初始化和页面分配算法。

## 小组成员

| 学号 | 姓名 | 负责内容 |
|------|------|----------|
| 2411333 | 吴卓彦 |`page_init()`、Buddy System、构建系统调查、报告整合 |
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

| 任务 | 状态 |
|------|------|
| `page_init()` | 已实现并提交；已记录 `make qemu` 的 `check_alloc_page()` 成功输出及页表虚实地址核对结果 |
| First-Fit 分析与 Best-Fit 实现 | 待完成；当前仍使用框架的 First-Fit，Best-Fit 仅有临时编译脚手架 |
| Buddy System 与测试 | 待实现 |
| SLUB 分配器与测试（Challenge） | 计划完成，尚未实现 |
| 页表分析、GDB 调试 QEMU、内存探测思考题 | 待补充成果与报告 |
| 构建系统调查 | 已分析部分评分流程，待整理完整报告 |
| 报告与提示词 | `report.md` 仍为模板；`prompt.md` 已记录 `page_init()` 的 Spec、提示词及迭代过程 |

## 运行与验证

实验环境为 WSL Ubuntu，使用 SiFive RISC-V GCC 10.2.0 工具链和 QEMU 4.1.1。先将 `riscv64-unknown-elf-` 工具链加入 `PATH`，再在 WSL 中进入本分支的 `code/` 目录：

```bash
make          # 编译、链接并生成内核镜像
make qemu     # 启动内核；退出时按 Ctrl+A，松开后按 X
make grade    # 在临时副本中注入测试并评分
```

GDB 调试时，在两个 WSL 终端的 `code/` 目录分别执行 `make debug` 和 `make gdb`。

目前 `make qemu` 使用 `default_pmm_manager`，成功输出 `check_alloc_page() succeeded!` 表示当前分配器自检通过。

## 提交要求

- 保留代码中的 `lab2` 任务注释，将 `YOUR CODE` 替换为实现者学号；不修改框架规定保留的测试函数。
- 编码任务在 `report/prompt.md` 中记录 Spec、最终提示词和真实迭代过程；Buddy 与 SLUB 分别记录实现提示词和测试提示词。
- 各模块报告包含功能与核心函数分析、测试证据、AI 生成代码的问题及修正，以及与 OS 原理知识点的对应关系；全组汇总原理中尚未在实验体现的知识点。
- 测试截图放入 `report/images/`，在 `report/report.md` 中使用相对路径引用；代码与报告提交到 `lab2` 分支。
