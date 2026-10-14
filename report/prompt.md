# Lab2 提示词汇总（prompt.md）

> lab2 包含代码编写任务，按老师要求，每个练习记录：**Spec（设计依据）→ 最终提示词 → 实现迭代过程**（AI 生成代码的 bug 与解决过程）。
> 由各练习负责人填写自己的部分。迭代过程**边做边记**：每轮记录现象（make qemu 实际输出）、原因分析、修正方式。

---

## 练习 1：page_init()（吴卓彦 2411333，kern/mm/pmm.c:64）

### 一、Spec（设计依据）

**1. 输入**
- get_memory_base() / get_memory_size()、链接符号 end（pmm.c 里 `extern char end[]`）
- 记 base = get_memory_base()，size = get_memory_size()
- 硬规则：代码里不许出现任何写死的数值（0x88000、31930 这些一个都不能出现）
- 可用工具：panic()、cprintf()、init_memmap()、pa2page()、链接符号 end、struct Page 及相关全局变量，以及框架提供的内存上界、地址转换、页面对齐和页面状态宏

**2. 输出：三个全局变量必须被正确设置**
- va_pa_offset = KERNBASE − KERNEL_BEGIN_PADDR（内核虚拟基址 − 内核物理装载地址，均来自 memlayout.h；数值上等于 PHYSICAL_MEMORY_OFFSET）
  - 【v4 更正】v3 原式 KERNBASE − base 是**配对错误**：VA−PA 必须用同一映射的两端相减——KERNBASE 的物理搭档是内核装载地址（KERNEL_BEGIN_PADDR = 0x80200000），不是 DRAM 起点（get_memory_base() = 0x80000000）。照抄会使 PADDR 全体低 2MB、空闲区起点落进 OpenSBI 保留区。由冷 AI 审查发现，详见迭代记录
- npage = PPN(min(base + size, PHYSICAL_MEMORY_END))
- pages = (struct Page *)ROUNDUP((uintptr_t)end, PGSIZE)
- 全局约束：正确处理可管理上界和页面对齐；内核及描述符数组不得进入空闲区；设备树信息无效时终止初始化；没有可用完整页面时不得调用 init_memmap()

**3. 过程（按执行顺序）**
1. 校验：DTB 无效（base 或 size 为 0）→ panic() 终止初始化
2. 给所有页 SetPageReserved()：覆盖页号范围 [nbase, npage) 的全部描述符
3. 计算空闲区：
   - 可管理总页数 = npage − nbase
   - 数组终点（虚拟）= pages + 可管理总页数（描述符指针加法）
   - 空闲区起点（物理）= PADDR(数组终点)，并 ROUNDUP 到页
   - 空闲页数 = npage − PPN(空闲区起点物理)
   - 防御：若 PPN(空闲区起点) ≥ npage（描述符数组撑出内存的极端情况），空闲页数记 0
4. 空闲页数 ≥ 1 时，调用 init_memmap(pa2page(空闲区起点), 空闲页数)
5. 用 cprintf 输出：base、size、空闲区起点、空闲页数

**4. 验收标准（make qemu 运行后串口按顺序出现）**
1. `memory management: default_pmm_manager`
2. `check_alloc_page() succeeded!`
3. satp 两行：两行不同；相减 = 0xFFFFFFFF40000000；物理行落在 0x8020xxxx 且末尾 000

**5. 禁止项**
- 不改任何测试函数（default_pmm.c / best_fit_pmm.c 中的 check）
- 不把保留区交给分配器
- 不写死任何数字

### 二、最终提示词

```text
【角色】
你是 RISC-V 教学内核（ucore lab2）的开发者。

【背景】
- 内核入口 entry.S 在进入 kern_init 前已建立 boot_page_table_sv39 并开启 Sv39
  分页（satp 已写、TLB 已刷），内核代码运行在高地址虚拟空间；
  虚拟地址转物理地址 = 减去 va_pa_offset。
- kern_init 里 dtb_init() 已执行，设备树解析出的内存信息通过
  get_memory_base() / get_memory_size()（kern/driver/dtb.h）获取。
- pmm_init 的调用顺序：init_pmm_manager()（已选定 default_pmm_manager）
  → page_init()（本任务）→ check_alloc_page()（会立刻尝试分配来验证成果）。
- default_init_memmap 有进门契约：交给它的每个页必须已设置 PG_reserved
  标志，否则 assert 失败。

【任务】
实现 pmm.c 中的 page_init()，规格如下。

1. 输入
- get_memory_base() / get_memory_size()、链接符号 end（pmm.c 里 extern char end[]）
- 记 base = get_memory_base()，size = get_memory_size()
- 硬规则：代码里不许出现任何写死的数值（0x88000、31930 这些一个都不能出现）
- 可以使用：panic()、cprintf()、init_memmap()、pa2page()、链接符号 end、
  struct Page 及相关全局变量，以及框架提供的内存上界、地址转换、
  页面对齐和页面状态宏

2. 输出：三个全局变量必须被正确设置
- va_pa_offset = KERNBASE − KERNEL_BEGIN_PADDR（= PHYSICAL_MEMORY_OFFSET）
- npage = PPN(min(base + size, PHYSICAL_MEMORY_END))
- pages = (struct Page *)ROUNDUP((uintptr_t)end, PGSIZE)
- 全局约束：正确处理可管理上界和页面对齐；内核及描述符数组不得进入
  空闲区；设备树信息无效时终止初始化；没有可用完整页面时不得调用
  init_memmap()

3. 过程（按执行顺序；先完成 ② 的三个赋值——下面的 PADDR 要用 va_pa_offset）
  1. 校验：DTB 无效（base 或 size 为 0）→ panic() 终止初始化
  2. 给所有页 SetPageReserved()：覆盖页号范围 [nbase, npage) 的全部描述符
  3. 计算空闲区：
    - 可管理总页数 = npage − nbase
    - 数组终点（虚拟）= pages + 可管理总页数（描述符指针加法）
    - 空闲区起点（物理）= PADDR(数组终点)，并 ROUNDUP 到页
    - 空闲页数 = npage − PPN(空闲区起点物理)
    - 防御：若 PPN(空闲区起点) ≥ npage（描述符数组撑出内存的极端情况），
      空闲页数记 0
  4. 空闲页数 ≥ 1 时，调用 init_memmap(pa2page(空闲区起点), 空闲页数)
  5. 用 cprintf 输出：base、size、空闲区起点、空闲页数

4. 验收标准（你无法运行，我会编译运行并反馈结果；供你理解成功标准）
make qemu 运行后串口按顺序出现：
  1. memory management: default_pmm_manager
  2. check_alloc_page() succeeded!
  3. satp 两行：两行不同；相减 = 0xFFFFFFFF40000000；
    物理行落在 0x8020xxxx 且末尾 000

5. 禁止项
- 不改任何测试函数（default_pmm.c / best_fit_pmm.c 中的 check）
- 不把保留区交给分配器
- 不写死任何数字

【代码环境】
参考文件：kern/mm/pmm.c、kern/mm/pmm.h、kern/mm/memlayout.h、
kern/mm/default_pmm.c、kern/driver/dtb.h（相关内容附后）。
只修改 pmm.c 中 "//lab2 function1 2411333" 标记处的 page_init 函数体，
该注释必须保留。

【交付】
完整的 page_init 函数代码（从 static void page_init(void){ 到对应的 }），
不改其他任何文件。

【协作方式】
你给出代码后，我编译运行并贴回实际输出；若与验收标准不符，我贴出
错误信息，你据此修正，直到通过。
```

> 【更正记录】实际发送版本中 ② 的 va_pa_offset 公式误写为 KERNBASE − base，冷 AI 审查后更正为上式；此处已是更正后版本，完整经过见下方迭代记录。发送时附上 pmm.h / memlayout.h / dtb.h 的实际内容，让 AI 看到宏的真实定义。

### 三、实现迭代过程（边做边记）

| 轮次 | 现象 / 报错（make qemu 实际输出） | 原因分析 | 修正 |
|------|------|------|------|
| v0（spec 审查） | 冷 AI 拒绝照抄 spec ② 公式：其结果 0xFFFFFFFF40200000 与验收 ④ 要求的 0xFFFFFFFF40000000 自相矛盾；证据链：Makefile loader 地址 0x80200000、kernel.ld 基址、框架三处一致的偏移常量、tools/test_lab2.c Test 1 断言 va_pa_offset == PHYSICAL_MEMORY_OFFSET（已亲验属实，test_lab2.c:20） | spec v3 配对错误：KERNBASE 的物理搭档是 KERNEL_BEGIN_PADDR（0x80200000）而非 get_memory_base()（0x80000000）。若照抄：PADDR 全体低 2MB，satp 物理地址会打在 0x8000xxxx，空闲区起点算进 OpenSBI 区，分配器放出的页覆盖固件与内核 | spec 更新为 v4：va_pa_offset = KERNBASE − KERNEL_BEGIN_PADDR（= PHYSICAL_MEMORY_OFFSET） |
| v1（代码运行） | make qemu 一次通过：check_alloc_page() succeeded!；free start = 0x80347000、pages = 31929；satp 0xffffffffc0205000 / 0x80205000，相减 = 0xFFFFFFFF40000000 | 全部验收通过 | 无需修正。free start 与旧参考值 0x80346000/31930 差一页：新代码使内核镜像增长（footprint 20KB→24KB）、end 从 0x80205078 跨页到 0x80206078，描述符数组整体后移一页——无硬编码，数值自适应，恰证"不写死数字"的意义 |

**实际输出（128MB QEMU，v1 运行实录）**：
base=0x80000000、size=0x8000000、空闲区起点=0x80347000、空闲页数=31929；satp 两行 0xffffffffc0205000 / 0x80205000（与 obj/kernel.sym 中 boot_page_table_sv39 一致）。这些数随内核大小浮动（end 跨页则整体后移一页），禁止出现在代码里。

---

## 练习 2：Best-Fit（kern/mm/best_fit_pmm.c:6）（负责人：＿＿＿）

（结构同上：Spec → 最终提示词 → 实现迭代过程。注意：basic_check/best_fit_check 两个测试函数不得修改；管理函数与 best_fit_pmm_manager 注册需自行设计；完成后须在 pmm.c 的 init_pmm_manager() 切换注册。）

## 练习 3：Buddy System（无框架，从零建文件）（吴卓彦 2411333）

（结构同上。手册要求两份 prompt：①参考现有 ucore 框架从零生成 Buddy 模块并接入内核 ②为该实现设计测试函数；报告需说明测试覆盖与有效性。）

## 选做：SLUB 分配器（若完成则记录）（负责人：＿＿＿）

（结构同上。两份 prompt：①生成模块 ②生成测试；可简化实现但须体现两层架构主体思想。）
