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

## 练习 2：Best-Fit（kern/mm/best_fit_pmm.c:6）（姚嘉乐 2413497）

### 一、Spec（设计依据）

- 沿用现有 `Page`、free_list、nr_free和双向链表，按地址维护空闲块，便于回收时合并相邻区域。
- 分配时选择满足请求的最小空闲块；大小相同时选地址较低者。支持整块分配、剩余块拆分和失败返回，准确维护页面标志及空闲页数。
- 保留 `basic_check()`、`best_fit_check()`，实现管理回调和注册对象，并在 `init_pmm_manager()` 中切换为 Best-Fit。

### 二、最终提示词

以下为实际使用的完整提示词：

````markdown
[PROMPT]

**任务**：
为当前内核实现并接入完整的 Best-Fit 连续物理页分配模块，使内核能够从所有满足请求的空闲块中，选择大小最小的一块分配连续物理页，并支持页面回收与相邻空闲块合并。

目标文件：
1. ucore-team/code/kern/mm/best_fit_pmm.c
   补全 Best-Fit 的内部状态、管理函数及 best_fit_pmm_manager 注册结构。
2. ucore-team/code/kern/mm/pmm.c
   仅修改 init_pmm_manager() 中的分配器选择，使内核使用 best_fit_pmm_manager。

**操作要求**：
- 必须修改项目中的实际文件，完成整个模块及其接入。
- 不允许修改 basic_check() 和 best_fit_check() 两个现有测试函数的全部内容。
- 允许替换 Best-Fit 文件中的临时编译脚手架，并添加模块内部的静态辅助函数、宏和变量。
- 保留现有代码，只在标记位置补全，不要删除既有逻辑。

**输出要求**：
- 使用下述 [RELY]、[GUARANTEE] 和 [SPECIFICATION] 完成真实文件修改，以工程真实代码为准，不要引入无关功能。
- 注意保持程序代码的可读性，合理使用注释。

[RELY]

请先读取两个目标文件，以及以下参考文件中的真实定义：

- ucore-team/code/kern/mm/pmm.h
- ucore-team/code/kern/mm/memlayout.h
- ucore-team/code/kern/mm/default_pmm.c
- ucore-team/code/kern/mm/best_fit_pmm.h
- ucore-team/code/libs/list.h

关键数据结构如下，使用工程中已有定义，不要重复定义：

```c
struct Page {
    int ref;
    uint64_t flags;
    unsigned int property;
    list_entry_t page_link;
};

struct pmm_manager {
    const char *name;
    void (*init)(void);
    void (*init_memmap)(struct Page *base, size_t n);
    struct Page *(*alloc_pages)(size_t n);
    void (*free_pages)(struct Page *base, size_t n);
    size_t (*nr_free_pages)(void);
    void (*check)(void);
};
```

[GUARANTEE]

必须实现以下模块内部接口：

```c
static void best_fit_init(void);
static void best_fit_init_memmap(struct Page *base, size_t n);
static struct Page *best_fit_alloc_pages(size_t n);
static void best_fit_free_pages(struct Page *base, size_t n);
static size_t best_fit_nr_free_pages(void);
```

必须在 Best-Fit 模块中定义具有外部链接的注册对象：

```c
const struct pmm_manager best_fit_pmm_manager;
```

注册对象的五个管理回调必须对应上述接口，check 回调必须绑定已有且未经修改的 best_fit_check()。

必须修改现有接口：

```c
static void init_pmm_manager(void);
```

修改目标是选择并初始化 best_fit_pmm_manager，保留原有的管理器名称输出和初始化调用。可以添加 static 辅助函数、模块内部宏及必要的内部状态，不改变公共接口和 Page 的结构定义。

[SPECIFICATION]

## best_fit_init

**Pre-Condition**：
- 内核处于物理内存管理器初始化阶段，尚未向本模块登记可用页面。

**Post-Condition**：
- 空闲链表处于合法的空链表状态。
- 空闲页计数为零。
- 模块可以接收后续的空闲区域登记。

## best_fit_init_memmap

**Pre-Condition**：
- 模块已经初始化。
- n > 0，base 指向描述符数组中的合法连续区间。
- 该区间内的页面均处于保留状态，且未被登记为空闲区域。

**Post-Condition**：
- 恰好新增 n 个可分配页面，空闲页计数增加 n。
- 新登记页面不再处于保留状态，引用计数为零。
- 页面被组织为符合模块状态约束的空闲块。

  **Case 1：没有相邻空闲块**
  - 新区域作为独立空闲块参与管理，其他空闲区域保持不变。

  **Case 2：存在相邻空闲块**
  - 新区域与实际连续的前后空闲块合并，形成完整的连续空闲区域。

**Requirements**：
- 新区域位于链表头部、中间或尾部时，均应正确维护双向链接。
- 每个空闲块节点只能挂接一次。
- 对 n == 0 的非法调用，沿用参考实现的断言检查。

## best_fit_alloc_pages

**Pre-Condition**：
- 模块已经初始化，n > 0。
- 当前链表中的空闲块具有合法的描述符和块长度。

**Post-Condition**：
- 成功时返回恰好 n 个连续物理页的首页描述符。
- 失败时返回 NULL，链表、标志和空闲页计数均保持调用前的状态。
- 所选块的页数是所有页数不少于 n 的空闲块中的最小值。
- 若有多个同样大小的最佳候选块，选择物理地址最低的一块。
- 分配区域从所选块的起始位置开始，空闲页计数减少 n。
- 返回区域不再属于空闲块，不具有空闲块首页标志；
- 页面引用计数保持为零，也不被标记为保留页。

  **Case 1：所选块恰好包含 n 页**
  - 整个块被分配，不留下该块对应的空闲节点。

  **Case 2：所选块大于 n 页**
  - 未分配的剩余部分继续作为连续空闲块参与管理，
    其首页、长度、标志和链表位置均正确。

  **Case 3：不存在足够大的连续空闲块**
  - 返回 NULL，包括总空闲页数不足，以及总量足够但分散在多个小块中的情况。

## best_fit_free_pages

**Pre-Condition**：
- 模块已经初始化，n > 0。
- base 指向合法的连续已分配页面区间。
- 待释放页面不属于保留区，也不属于当前空闲区域。

**Post-Condition**：
- 恰好归还 n 个页面，空闲页计数增加 n。
- 归还页面的引用计数为零。
- 释放后的空闲区域满足模块状态约束。

  **Case 1：前后均无相邻空闲块**
  - 释放区域成为独立空闲块。

  **Case 2：仅前侧或仅后侧存在相邻空闲块**
  - 释放区域与该相邻块合并，首页和块长度准确。

  **Case 3：前后均存在相邻空闲块**
  - 三段区域合并为一个连续空闲块，只保留最终块的首页节点与标志。

## best_fit_nr_free_pages

**Pre-Condition**：
- 模块已经初始化。

**Post-Condition**：
- 返回当前维护的空闲页计数。

## best_fit_pmm_manager 注册对象

**Pre-Condition**：
- 各管理接口已经实现，现有 best_fit_check() 保持原样。

**Post-Condition**：
- name 为 "best_fit_pmm_manager"。
- 五个管理回调准确绑定本模块对应接口。
- check 绑定现有 best_fit_check()。
- 对象与头文件中的 extern 声明一致，可以被通用层引用。

## init_pmm_manager

**Pre-Condition**：
- 由 pmm_init() 在登记空闲页面之前调用。

**Post-Condition**：
- pmm_manager 指向 best_fit_pmm_manager。
- 输出当前管理器名称，并通过其 init 回调完成初始化。
- 后续空闲页登记、分配、释放及检查均进入 Best-Fit 模块。

````

### 三、实现迭代过程

依照提示词，首轮生成编译后就通过了相关测试。已使用 `make grade` 验证，结果见 [Best-Fit 测试截图](./images/Bestfit算法替换后的测试截图.png)。

## 练习 3：Buddy System（无框架，从零建文件）（吴卓彦 2411333）

### 一、Spec（设计依据）

> 以下规格在实现前讨论确定。两份提示词已分别用于生成模块和独立测试；独立测试经快照补强及断言修正后，用户重跑确认四组通过，随后现有 SLUB 六组测试也通过。向上取整和整块释放规则是本实现的设计选择。

**1. 接口与输入输出**

- 接入 `struct pmm_manager`，提供 init、init_memmap、alloc_pages、free_pages、nr_free_pages、check 六个接口。
- `init_memmap(base, n)` 的 base 为起始物理页对应的 `struct Page *`，n 为连续可用页数；可用区域由现有 page_init 提供。
- `alloc_pages(n)` 成功时返回分配块首页的描述符指针，失败时返回 NULL；该返回方式参考现有 default_pmm.c 实现和框架失败测试。
- `page2pa(p)` 计算描述符 p 对应物理页的起始物理地址，不建立页表映射。

**2. 初始化与编号基准**

- init 将各阶空闲链表初始化为空，将空闲页总数设为 0；必须在登记可用页之前调用。
- order=k 表示一块包含 `2^k` 个连续物理页，各阶链表可有零块或多块。
- 本草稿统一采用绝对物理页号 `page2ppn(p)` 判断块对齐和计算伙伴；不将 Page 数组下标或空闲区域相对编号混入计算。前述从第 0 页开始的例子用于说明算法。
- init_memmap 从区域起点开始，逐步选取满足“起始物理页号可被块页数整除，且块完全落在剩余可用区域内”的最大合法块，登记至对应阶链表；直到 n 页全部登记，空闲页总数增加 n。
- 不丢弃非 2 的整数次幂长度区域的尾页，不将区域外页面、内核或描述符数组等占用页面登记为空闲。沿用框架的保留页交接约定：确认交接页为 Reserved 后，初始化其状态、引用计数与块信息。

**3. 分配规则**

- 正常请求 n 必须大于 0。采用整数运算求满足 `2^order >= n` 的最小阶数，不使用浮点 log 函数。
- 从目标阶开始向更高阶查找，找到第一个非空链表后取出一块，逐级对半拆分；每次将一半留作继续拆分或最终分配，另一半登记到低一阶空闲链表。
- 没有足够大的块时返回 NULL；即使空闲页总数不少于 n，也不能保证存在合适的连续块。失败不改变空闲状态和计数。
- 成功后从空闲页总数中扣除实际块页数 `2^order`，记录分配块的大小，返回首页描述符指针。取整或阶数超出实现支持范围的请求必须安全失败。

**4. 释放与合并规则**

- free_pages(base, n) 使用原分配返回的首页描述符和原请求页数；与分配相同地向上取整，归还整个块。free_page(p) 是 free_pages(p, 1) 的单页接口。
- 释放前核对块确实已分配、起点和大小有效且与已记录的分配信息匹配；错误释放不得破坏空闲链表。
- 空闲页总数只增加本次实际归还的块页数；之后合并不额外增加计数。
- 伙伴起始物理页号为 `当前起始物理页号 ^ (1UL << order)`。公式只给出候选位置，不保证那里有可合并的空闲块。
- 在访问候选伙伴描述符前检查范围。只有整个伙伴块属于已登记的可用区域，而且其首页标记为同阶完整空闲块，才可合并；不得跨越未登记区域或保留页。
- 从空闲链表移除伙伴，以两个块中较低的起始页号作为新块起点，order 加 1，并继续尝试；最终将合并结果登记到对应阶链表。

**5. 接入方案与验收设计**

- 通过单独的编译开关选择 Buddy，默认构建保持当前管理器选择；保留队友 Best-Fit 的实现及课程评分接入能力。切换编译开关后先 clean 再重新构建，避免复用旧目标文件。
- 不修改 First-Fit、Best-Fit 的框架测试来适配 Buddy；Buddy 需要独立测试。
- 以下为预期行为，不是已运行结果。涉及固定页数和编号的场景应在受控测试区域中验证；不能假定整机只有这些空闲页，也不能把教学编号直接当作实际物理页号。

| 场景 | 预期结果 | 验证目的 |
|------|----------|----------|
| 初始登记从对齐编号 0 开始的 19 页 | 16+2+1 页，共 19 页，无尾页丢失 | 非 2 的整数次幂区域初始化 |
| 登记统一编号基准下的第 4～15 页 | 4～7 为 4 页块，8～15 为 8 页块 | 非大块边界起点的对齐处理 |
| 16 页区域依次申请 3 页和 5 页 | 实际扣除 4、8 页，剩余 4 页 | 请求向上取整与计数 |
| 将上例两块全部归还，再申请 16 页 | 空闲数恢复 16，且完整 16 页申请成功 | 拆分与递归合并；不能只检查计数 |
| 仅有 16 页时申请 17 页 | 返回 NULL，空闲页数仍为 16 | 总量不足时失败无副作用 |
| 将 16 页分配为四个 4 页块，仅释放 0～3 和 8～11，再申请 8 页 | 返回 NULL，空闲页数仍为 8 | 总量足够但连续块不足 |
| 接上例释放 4～7，再申请 8 页 | 0～3 与 4～7 合并；申请成功，剩余空闲 4 页 | 伙伴释放后重新满足大块请求 |
| 候选伙伴越界、已分配或不是同阶完整空闲块 | 不越界访问、不错误合并 | 伙伴状态与边界检查 |

测试结束后应归还所有测试分配，恢复测试前可用页数与正常分配能力。错误释放、超大请求及支持阶数边界的具体测试将在第二份提示词中补充。

### 二、生成实现的提示词

```text
[PROMPT]

任务：
在当前 RISC-V uCore Lab2 框架中，从零实现并接入 Buddy System 物理页
分配器，支持按 2 的整数次幂分配连续物理页、逐级拆分和伙伴合并。
项目根目录为 E:\大三上\OS\OS2026_Labs，以下路径均相对于该目录。

修改范围：
- 新建 code/kern/mm/buddy_pmm.c 和 buddy_pmm.h。
- code/kern/mm/pmm.c 仅增加必要头文件和 init_pmm_manager 的条件选择。
- code/Makefile 增加 BUDDY=1 时定义 USE_BUDDY_PMM 的编译开关。
- 未启用开关时保留真实代码中现有的管理器选择，不预设它一定是 First-Fit；
  Buddy 的加入不得覆盖队友已接入或后续接入的 Best-Fit。

操作与交付要求：
- 先阅读真实代码并审查规格；发现矛盾时说明证据和修正理由。
- 修改实际文件，不修改 page_init、公共 Page 布局、其他分配器、既有测试、
  评分脚本或报告；保留原有初始化流程和 lab2 注释，新增任务代码注明 2411333。
- 可以添加模块内部 static 辅助函数、宏和状态，不使用 malloc 或依赖
  尚未初始化的分配器来分配元数据。
- 说明修改文件、元数据含义、对齐与伙伴算法、分配和释放约定。
- 本轮提供基础 check；完整验收测试由下一份独立提示词生成。
- 给出先 make clean、再 make BUDDY=1 qemu 的步骤与预期检查点；恢复默认
  构建前也需 make clean。不要替我运行 make、QEMU、GDB 或提交 Git。
- 未运行的检查明确标为预期，不编造编译或测试通过记录。

[RELY]

请先读取以下文件的真实定义：
- code/kern/mm/pmm.h、pmm.c、memlayout.h、mmu.h、default_pmm.c。
- code/kern/mm/best_fit_pmm.h，及 pmm.c 当前实际使用的其他管理器头文件。
- code/libs/list.h、defs.h，code/Makefile、code/tools/function.mk。

框架已有 struct Page 和 struct pmm_manager，直接复用，不重复定义或改变布局。
可以使用现有链表接口、页状态宏、page2ppn/page2pa、引用计数接口、assert
和 cprintf；以实际声明为准。

环境与调用约定：
- page_init 已实现；pmm_init 依次选择并初始化管理器、调用 page_init 登记
  可用页、调用管理器 check。此时尚无其他并发分配者。
- init_memmap(base, n) 的 base 是起始页的描述符指针，n 是连续可用页数。
  页面交接时带有 PG_reserved；模块登记后才解除相应可用页的保留状态。
- nbase 和 npage 界定描述符覆盖的物理页号区间，pages 是描述符数组地址。
  描述符存在不代表对应物理页可以分配，仍须检查该页是否已登记为可用。
- page2ppn 返回绝对物理页号，page2pa 返回页起始物理地址；伙伴计算和
  对齐统一使用绝对物理页号，不混用数组下标或区域相对编号。
- 分配失败返回 NULL，依据现有分配器实现及框架失败测试。

[GUARANTEE]

必须实现以下模块内部接口：
static void buddy_init(void);
static void buddy_init_memmap(struct Page *base, size_t n);
static struct Page *buddy_alloc_pages(size_t n);
static void buddy_free_pages(struct Page *base, size_t n);
static size_t buddy_nr_free_pages(void);
static void buddy_check(void);

必须在 buddy_pmm.c 定义具有外部链接的注册对象：
const struct pmm_manager buddy_pmm_manager;

在 buddy_pmm.h 声明：
extern const struct pmm_manager buddy_pmm_manager;

注册对象的六个回调绑定上述对应函数。保留现有公共分配/释放接口不变。
init_pmm_manager 在 USE_BUDDY_PMM 启用时选择 buddy_pmm_manager，
否则沿用原选择；保留管理器名称输出和 init 调用。

[SPECIFICATION]

全局不变量：
- order=k 的空闲链表只包含完整、空闲、按绝对物理页号对齐的 2^k 页块。
- 每个空闲块只登记其首页节点；空闲块互不重叠，且全部属于已登记可用页。
- 空闲页总数等于各阶空闲块实际页数之和；分配块与空闲块不重叠。
- 所有移位、向上取整与范围计算避免溢出；检查范围后才访问描述符。

buddy_init
Pre-Condition：管理器初始化阶段，尚无待保留的存活分配。
Post-Condition：各阶空闲链表合法且为空，计数为 0，内部状态可接收区域登记。

buddy_init_memmap
Pre-Condition：模块已初始化；n>0，base 指向合法的连续 n 个页描述符；
这些页尚未登记且全部为框架交接的保留页，不含内核或描述符数组占用页。
Post-Condition：
- 恰好登记这 n 页，空闲数增加 n，正确设置标志、引用计数和块元数据。
- 从起点逐步选择同时满足物理页号对齐和剩余长度限制的最大合法块，
  不越界、不丢弃非 2 的整数次幂区域的尾页。
- 对可合并的同阶空闲伙伴进行必要合并，不跨越保留或未登记的页。

buddy_alloc_pages
Pre-Condition：模块已初始化且可用区域已登记；n>0，零请求以 assert 拒绝。
Post-Condition：
- 用整数运算求满足 2^order>=n 的最小阶数。
- 从目标阶向更高阶查找首个非空链表，取一块并逐级对半拆分；每次将另一半
  放入相应低阶链表，直到取得目标阶块。
- 成功返回该块首页描述符，记录原请求及实际块大小，空闲数减少 2^order。
- 无合适块、超出支持阶数或请求取整无法安全表示时返回 NULL；失败时
  空闲链表、计数和已有分配不变。总空闲数足够不代表连续块足够。

buddy_free_pages
Pre-Condition：base 是尚未释放的原分配首页，n 等于原申请页数且 n>0。
Post-Condition：
- 与分配时相同地向上取整，归还整个块，空闲数只增加实际归还页数一次。
- 按 伙伴物理页号=当前起始物理页号^(1UL<<order) 计算候选位置。
- 确认候选整块在已登记范围内，且首页代表同阶完整空闲块，才能合并。
- 移除伙伴旧链表节点，以较低起始物理页号作为新起点，order 加 1，继续
  尝试；不能再合并时将结果加入对应链表。合并不再次增加空闲数。
Requirements：释放前核验原分配记录；重复释放、错误起点、请求量不匹配
等错误按 assert/panic 风格拒绝，必须在破坏链表或计数之前拒绝。

buddy_nr_free_pages
Pre-Condition：模块已初始化。
Post-Condition：准确返回空闲物理页总数，不修改分配器状态。

buddy_check
Pre-Condition：真实可用内存已登记，没有并发分配者。
Post-Condition：检查各阶块大小、对齐、范围、标志和计数等基础不变量；
如进行分配/释放检查，须回收所有测试分配并恢复入口空闲数及分配能力。
检查失败以断言定位；基础检查成功不得冒充尚未生成的完整测试已通过。

init_pmm_manager 与编译接入
Pre-Condition：内核处于管理器选择阶段。
Post-Condition：BUDDY=1 定义 USE_BUDDY_PMM 并选择 Buddy，其余情况保留
原管理器；两个模式均保留名称输出和 init 调用。使用已有目录编译机制
构建新模块，不污染课程评分注入点。
```

### 三、生成测试的提示词

> 以下为已发送的第二份提示词，保留发送时版本。生成后另用下方两份补充提示词进行测试补强和断言修正，真实结果见迭代记录。

```text
[PROMPT]

任务：
为已经实现的 RISC-V uCore Lab2 Buddy 分配器生成独立的针对性测试，
检验初始化、对齐、取整、拆分、合并、碎片化失败及状态恢复。
项目根目录为 E:\大三上\OS\OS2026_Labs，以下路径均相对于该目录。

修改范围与操作要求：
- 仅在 code/kern/mm/buddy_pmm.c 中增加 static 测试辅助函数，并在原有
  buddy_check() 中接入；保留已有基础检查和真实内存分配/释放检查。
- 不修改分配算法、元数据语义、公共接口、其他分配器、SLUB、评分脚本或报告。
- 测试按规格独立构造预期值，不复制错误实现作为预期；若发现实现问题，
  保留失败并报告最小触发场景、期望与实际表现，交由后续独立修正。
- 新增测试注释含 lab2 和学号 2411333。说明每组测试的检验依据及局限。
- 不替我运行 make、QEMU、GDB 或提交 Git；给出用户执行步骤和预期输出。
  不编造运行结果，也不要把已有基础自检通过写成新增测试通过。

[RELY]

先读取：
- code/kern/mm/buddy_pmm.c、buddy_pmm.h、pmm.c、pmm.h、memlayout.h、mmu.h。
- code/libs/list.h、defs.h，code/kern/debug/assert.h、code/libs/stdio.h。
- code/kern/init/init.c，确认 Buddy check 发生在 SLUB 初始化之前。
- report/prompt.md 中练习 3 的 Spec 与第一轮真实运行记录。

已确定的行为：
- order=k 的块实际占用 2^k 页；alloc(n) 向上取整，free(p,n) 使用原请求。
- 以绝对物理页号判断对齐和计算异或伙伴，失败返回 NULL 且不改变状态。
- Buddy 私有链表和计数在同一源文件内可供 static 测试函数检查；框架现有
  assert/cprintf 可用，不需要导出这些私有状态。
- 正常启动无并发分配者，但不能损坏真实内存分配器状态或后续 SLUB 测试。

测试隔离依据：
- 固定 16/19 页的情景不能直接用整机空闲池，否则其余内存会掩盖预期失败。
- 可在同一模块内建立有界 static struct Page 模拟数组，仅测试描述符算法。
  临时使用该数组作为 pages，并把 npage 设为 nbase+模拟页数；nbase 是 const，
  不得修改。先确认其实际值满足测试区域所需对齐，按绝对页号计算期望。
- 必须完整保存并恢复真实 pages、npage、各阶链表头和空闲计数。复制出的真实
  哨兵仅作为恢复快照，不可当作已迁移的可遍历链表使用；模拟测试期间只访问
  模拟描述符，不触及真实链表节点或其页状态。不临时改 va_pa_offset。
- 模拟页表述的是测试描述符，其 page2pa 计算值不能用来读写真实物理内存。
  若采用其他隔离方案，先说明其状态保存与恢复为何完整，再实现。

[GUARANTEE]

新增以下模块内测试入口，可以添加必要的 static 辅助函数和有界静态数组：
static void buddy_test_init_ranges(void);
static void buddy_test_split_merge(void);
static void buddy_test_fragmentation(void);
static void buddy_test_failure_state(void);

在现有 static void buddy_check(void) 中调用上述测试，确保其运行时管理器
为 Buddy，测试后恢复真实环境，再完成真实分配器基础一致性复查。
不重复初始化 SLUB，不删除原有 check 的有效检查。

[SPECIFICATION]

所有固定编号均指测试基准附近的相对偏移；实际对齐和伙伴断言使用绝对页号。
模拟环境每组开始前独立初始化，未登记的模拟页始终保持 Reserved。

buddy_test_init_ranges
Pre-Condition：受控模拟环境已建立，测试数组至少包含 32 个描述符。
Post-Condition：
- 对 16 页对齐的起点登记 19 页，验证形成 16+2+1 页，计数为 19，尾页可用。
- 重建空环境，只登记偏移 4～15 的 12 页，验证 4～7 是 order 2，
  8～15 是 order 3；其余页没有进入链表。
- 分两次登记相邻伙伴区域，例如先 0～3 再 4～7，验证最后可申请完整 8 页。
- 覆盖合法的单页范围尾部，候选伙伴越界或未登记时不可误合并或越界访问。

buddy_test_split_merge
Pre-Condition：模拟环境仅登记对齐的 16 页。
Post-Condition：
- 分配 3 页成功，实际扣除 4 页，物理页号按 4 页对齐；再申请 5 页成功，
  实际扣除 8 页，按 8 页对齐；两个实际分配区间不重叠，剩余 4 页。
- 按原请求量释放两块，空闲数恢复 16；随后申请 16 页成功，证明确实合并。
- 归还该块并验证完整状态恢复。申请返回地址不作无依据的固定顺序假设。

buddy_test_fragmentation
Pre-Condition：模拟环境仅登记对齐的 16 页。
Post-Condition：
- 分配四块各 4 页，通过返回块的物理地址排序确定实际空间顺序。
- 释放排序后的第一、第三块，计数为 8；申请 8 页返回 NULL，计数和各阶
  空闲块集合与失败前完全相同，仍存活的分配记录保持有效。
- 释放第二块，与第一块合成 8 页；申请 8 页成功，剩余空闲数为 4。
- 归还新块和仍存活的第四块，恢复完整 16 页，并再次成功申请和归还 16 页。

buddy_test_failure_state
Pre-Condition：受控模拟环境已登记对齐的 16 页。
Post-Condition：
- 申请 17 页、超过实现支持最大块的请求、接近 size_t 上限的请求均返回 NULL。
- 失败前后空闲数、各阶空闲块集合相同，不能只凭总计数判断无副作用。
- 保留一个合法存活分配时再次进行超大请求，确认其分配记录未改变且仍可
  正常按原请求释放；随后普通分配和释放仍成功。

测试输出与恢复要求：
- 每组全部检查完成后才输出对应 [BUDDY] ...: PASS；仅四组全部成功且真实
  分配器状态已恢复并复查后输出 [BUDDY] independent tests passed!。
- 不在这次正常启动路径中直接触发预期会 panic 的零请求或非法释放。
  输出说明这些拒绝路径尚未独立动态验证，不能声称已覆盖。
- 函数返回时无测试存活分配，恢复真实 pages/npage、链表和计数；确保后续
  SLUB 测试仍能使用真实物理页。
- 编译步骤为用户先 make clean，再 make BUDDY=1 qemu；说明预期标记，
  实际结果留待用户反馈。
```

### 四、测试迭代的补充提示词

**补充一：失败状态快照检查补强（已发送）**

```text
[PROMPT]
补强 Buddy 测试中“分配失败不改变状态”的检查，仅修改测试辅助代码。

[RELY]
读取 buddy_pmm.c 中 buddy_test_state_save 和
buddy_test_state_assert_same。当前只保存各阶哨兵指针和计数，
不足以验证模拟页描述符及链表内部节点未改变。

[GUARANTEE]
保留现有测试入口和分配算法，扩充状态快照与比较。
使用有界静态存储，避免扩大内核栈占用。

[SPECIFICATION]
保存并逐项比较：
1. 空闲页计数及各阶哨兵的 prev/next；
2. 全部模拟页的 flags、property、ref；
3. 全部模拟页 page_link 的 prev/next。
不要使用包含结构体填充字节的整块内存比较。
所有项目一致才判定失败无副作用，并修正原有过度概括的注释。
不要运行 make/QEMU，由我执行。
```

**补充二：修正相互矛盾的测试断言（已发送）**

```text
[PROMPT]
修正 Buddy 独立测试首次运行中发现的断言错误。
仅修改测试代码，不修改分配器算法。

[RELY]
用户实际运行 make clean 后执行 make BUDDY=1 qemu：
[BUDDY] 1 init ranges: PASS
随后在 buddy_pmm.c:528 触发：
assertion failed: buddy_test_order_blocks(k) == 0

请读取 code/kern/mm/buddy_pmm.c 中
buddy_test_split_merge() 的实际代码。

当前先断言 order 4 有一个块，随后又在 k=0..14
的循环中要求各阶均为空。循环包含 order 4，
导致测试条件相互矛盾。

[GUARANTEE]
修正 buddy_test_split_merge() 末尾的空闲链表状态检查。
保留现有测试入口、其他有效断言和分配器实现。
不得删除失败检查来绕过问题。

[SPECIFICATION]
前置条件：
受控区域只有对齐的16页，分配块已全部归还。

后置条件：
1. order 4 恰好有一个完整的16页空闲块。
2. 遍历 0 至 BUDDY_MAX_ORDER，包含最大阶；
   对所有 k != 4 的阶，断言空闲块数为0。
3. 保留区域起点、块大小及链表归属的现有检查。

说明修改位置及原因。
不要替我运行 make/QEMU，不提交 Git，不修改报告；
由我重新运行并反馈真实结果。
```

### 五、实现迭代与实际运行结果

| 轮次 | 实际现象 | 分析与处理 |
|------|----------|------------|
| v1：模块首次生成与运行 | 用户在 WSL 执行 make clean 后执行 make BUDDY=1 qemu；Buddy 编译、链接及启动成功；基础自检和现有 SLUB 六组测试全部通过 | 本轮未根据编译或运行报错修改代码；说明模块已接入并能支撑这些实际调用，独立针对性测试仍待生成与运行 |
| v2：独立测试首次实际运行 | 测试生成后，审查发现失败状态快照只比较哨兵及计数，先补强为全部模拟页逐字段比较；随后用户运行，组 1 init ranges 为 PASS，组 2 在 buddy_pmm.c:528 断言 buddy_test_order_blocks(k) == 0 失败 | 测试预期自相矛盾：前一行要求 order 4 有 1 块，紧接着 k=0..14 循环又要求包含 order 4 在内的各阶都为空。通过补充提示词让实现 AI 保留 order 4 恰有 1 块的检查，并遍历全部阶数，仅对 k!=4 检查为 0。本轮在组 2 中止，组 3、组 4 和后续 SLUB 未执行到 |
| v3：测试断言修正后重跑 | 2026-10-23 用户重新构建并运行，init ranges、split/merge、fragmentation、failure state 四组均 PASS；输出 independent tests passed、check_alloc_page succeeded，随后 SLUB 六组均 PASS | 改动仅修正测试断言，未通过更改分配算法或删除有效检查来绕过失败；正常路径的独立测试与真实内存回环通过。零请求和非法释放的预期 panic 路径未独立动态验证 |
| 回归验证：默认模式与评分 | 2026-10-23 用户切回默认构建运行，输出 best_fit_pmm_manager、check_alloc_page succeeded 及 SLUB 六组 PASS；随后 make grade 的 page init 与 best fit 均 OK，总分 20/20 | Buddy 编译开关保留默认 Best-Fit 路径，加入模块及测试后课程评分仍通过；20/20 为课程这两项评分结果，Buddy 的独立测试证据单独保留 |

**v2 失败证据**：[独立测试首次运行断言失败](./images/buddy-v2-test-assertion-failure.png)。截图实际显示空闲区起点 0x8034b000、31925 页；内核增加测试与静态快照后占用扩大。这些布局观测值不作为固定验收常量。

**v1 运行证据**：用户提供的 [编译与启动截图](./images/buddy-v1-build.png)、[Buddy 基础自检及 SLUB 测试截图](./images/buddy-v1-output.png)。

截图中的实际输出：

```text
memory management: buddy_pmm_manager
free memory: start = 0x0000000080348000, pages = 31928
buddy_check(): basic invariants and alloc/free round-trip passed
check_alloc_page() succeeded!
satp virtual address: 0xffffffffc0206000
satp physical address: 0x0000000080206000
[SLUB] 1 zero/null: PASS
[SLUB] 2 size boundaries / large objects: PASS
[SLUB] 3 data isolation: PASS
[SLUB] 4 cross-slab reuse: PASS (3 slabs, 125 reused slots)
[SLUB] 5 physical page reclaim: PASS
[SLUB] 6 allocation failure / overflow: PASS
[SLUB] all tests passed!
```

与 page_init 首次运行时的 31929 页相比，本次为 31928 页；截图显示内核 end 已到 0xffffffffc0207e2c，描述符数组与空闲区随镜像布局后移。上述地址和页数为本轮观测值，不作为固定验收常量。该记录不是 Buddy 独立测试或默认 Best-Fit 模式重新评分通过的记录。

**v3 最终运行证据**：[Buddy 独立测试和后续 SLUB 测试通过截图](./images/buddy-v3-independent-tests-pass.png)。截图显示：

```text
memory management: buddy_pmm_manager
free memory: start = 0x000000008034b000, pages = 31925
[BUDDY] 1 init ranges: PASS
[BUDDY] 2 split/merge: PASS
[BUDDY] 3 fragmentation: PASS
[BUDDY] 4 failure state: PASS
buddy_check(): basic invariants and alloc/free round-trip passed
[BUDDY] note: zero-request / illegal-free rejection paths panic by design and are NOT dynamically verified here
[BUDDY] independent tests passed!
check_alloc_page() succeeded!
satp virtual address: 0xffffffffc0208000
satp physical address: 0x0000000080208000
[SLUB] all tests passed!
```

截图还包含 SLUB 六组各自的 PASS 行。独立测试使用静态模拟描述符隔离固定规模的页分配场景，返回前恢复真实环境；随后的真实页回环检查与 SLUB 测试提供接入验证。测试及静态记录增加了镜像占用，最终空闲数为 31925 页，不应照抄旧值作为判定条件。

**默认模式回归证据**：[Best-Fit 与 SLUB 运行通过](./images/buddy-regression-bestfit-slub-pass.png)、[make grade 20/20](./images/buddy-regression-grade-20.png)。用户在 Buddy 验证后先执行 make clean，再执行默认 make qemu，确认管理器为 best_fit_pmm_manager，基础检查及 SLUB 六组测试通过；退出 QEMU 后执行 make grade，实际结果为：

```text
lab2 injection complete
  -check page init: OK
  -check best fit: OK
Total Score: 20/20
```

该评分检查 page_init 与 Best-Fit，不替代前述 Buddy 四组独立测试。两种运行模式的验证均已保留各自截图。

## 选做：SLUB 分配器（姚嘉乐 2413497）

### 一、Spec（设计依据）

- 采用两层架构：底层复用物理页分配器，上层将页面划分为不同大小的对象槽位；大对象直接申请连续物理页。
- 对象至少按 16 字节对齐，优先复用已有空位；空 slab 立即归还页面，大对象释放时完整回收，失败时保持已有对象有效。
- 模块和测试分别生成。测试通过公开接口检查返回地址、数据内容及空闲页数，不依赖私有管理结构。

### 二、最终提示词

**提示词 1：生成模块**

````markdown
[PROMPT]

**任务**：
为当前 ucore Lab2 内核实现简化的 SLUB 内存对象分配模块，体现两层架构：底层使用现有物理页分配器，上层将页面组织为对象池，提供按字节申请和释放内存的接口。

目标文件：
- 新增 code/kern/mm/slub.h，声明公开接口。
- 新增 code/kern/mm/slub.c，实现对象分配与释放。
- 修改 code/kern/init/init.c，在物理内存初始化完成后初始化 SLUB。

**操作要求**：
- 必须修改项目中的实际文件，完成模块及其接入。
- 仅修改上述文件，保留已有实现，不修改物理页分配器、公共数据结构、构建规则和现有测试。
- 本次只实现模块，不新增测试函数；测试由后续独立任务完成。
- 可以自行设计对象大小类别、内部管理结构及 static 辅助函数。
- 在新增实现处添加 lab2 任务注释，注明学号 2413497。

**输出要求**：
以工程真实定义和下述规格为准。不要引入无关功能。注意维护代码的可读性，多使用注释。


[RELY]

请先读取以下文件的相关定义，直接使用已有接口，不重复实现：

- code/kern/mm/pmm.h：
  物理页分配、释放、空闲页统计及地址转换接口。
- code/kern/mm/mmu.h：
  PGSIZE 等页面大小定义。
- code/libs/defs.h：
  基础类型、ROUNDUP、ROUNDDOWN 和 to_struct 等宏。
- code/libs/list.h：
  list_entry_t 及双向循环链表操作。
- code/libs/string.h：
  memset、memcpy 等内存操作接口。
- code/kern/debug/assert.h：
  assert、panic 等检查接口。

主要物理页接口：

```c
struct Page *alloc_pages(size_t n);
void free_pages(struct Page *base, size_t n);
size_t nr_free_pages(void);
```

接入与构建条件：
- kern_init() 已调用 pmm_init()；SLUB 应在该调用完成后、进入主循环前初始化。
- 现有构建规则会自动收集 code/kern/mm/ 下的 C 文件，


[GUARANTEE]

必须实现以下公开接口，并在头文件中声明：

```c
void slub_init(void);
void *slub_alloc(size_t size);
void slub_free(void *ptr);
```

必须修改现有函数：

```c
int kern_init(void);
```

修改目标：在 pmm_init() 完成后调用 slub_init()，保留原有启动流程。

允许添加模块内部的 static 辅助函数、宏和管理状态，不改变现有物理内存管理接口。


[SPECIFICATION]

## 模块整体要求

- 小对象按大小分类，将 slab 中的数据空间组织为固定尺寸的对象槽位。
- 优先复用已有 slab 中的空闲对象，没有可用对象时再申请物理页。
- 超过小对象类别上限的请求，通过足够数量的连续物理页满足。
- 不同的存活分配区域不得重叠，分配和释放不得破坏其他存活对象的数据。
- 返回给调用者的有效区域，不得包含仍需保留的管理信息。
- 内部布局和辅助函数由实现自行设计，时空复杂度不作额外要求。


## slub_init

**Pre-Condition**：
- 物理内存管理已经初始化，本模块尚未初始化。

**Post-Condition**：
- 内部管理状态初始化完成，可以接受后续分配和释放请求。
- 此时没有存活对象，不预先占用物理页。


## slub_alloc

**Pre-Condition**：
- 本模块已经初始化。

**Post-Condition**：
- 成功时返回至少包含 size 个可用字节、满足 16 字节对齐的内核虚拟地址。
- 失败时返回 NULL，不泄漏本次申请的资源，不破坏已有分配及管理状态。

  **Case 1：size 为 0**
  - 返回 NULL，状态不变。

  **Case 2：请求能够满足**
  - 小对象通过对应大小类别的 slab 分配，较大对象通过整页分配路径取得。
  - 调用者可以在请求长度范围内正常读写，之后通过 slub_free() 释放。
  - 不要求返回的数据已经清零。

  **Case 3：内存不足或长度计算溢出**
  - 返回 NULL，已有对象保持有效，管理状态保持正确。


## slub_free

**Pre-Condition**：
- 本模块已经初始化。
- ptr 为 NULL，或本模块返回且尚未释放的原始指针。

**Post-Condition**：
- ptr 为 NULL 时不进行操作。
- 小对象释放后可以再次用于分配；所属 slab 全部空闲时，立即将其物理页归还底层。
- 大对象释放时，归还本次分配占用的全部物理页。
- 释放所需的归属和大小信息由模块维护，不要求调用者额外传入长度。
- 其他存活对象保持有效，空闲对象、使用计数及链表状态一致。
- 所有对象释放后，模块不保留动态申请的物理页。


## kern_init

**Pre-Condition**：
- 内核正在执行启动初始化。

**Post-Condition**：
- 先完成现有 pmm_init()，再初始化 SLUB。
- 不改动其他内容，保留原有物理页检查、信息输出及启动流程。

````

**提示词 2：生成测试**

````markdown
[PROMPT]

**任务**：
为当前已实现的简化 SLUB 分配器编写独立测试，验证不同大小对象的分配、读写、释放、复用和物理页回收。

项目根目录为 ucore-team，以下路径均相对于该目录。

目标文件：
- 新增 code/kern/mm/slub_test.h，声明测试入口。
- 新增 code/kern/mm/slub_test.c，实现测试。
- 修改 code/kern/init/init.c，在现有 slub_init() 调用后执行测试。

**操作要求**：
- 必须修改项目中的实际文件，完成测试及其接入。
- 仅修改上述文件，不修改 SLUB 实现、物理页分配器、已有测试和构建规则。
- 测试依据接口规格判断结果，不将实现中的错误作为预期行为。
- 发现问题时保留失败并报告触发条件，不擅自修复被测模块。
- 允许添加 static 辅助测试函数和有界的静态记录数组。
- 新增测试注明 lab2 和学号 2413497。

**输出要求**：
参考真实工程实现和下述规格。说明各组测试覆盖的行为及其检验依据，编译并运行，报告实际结果；未执行的验证如实说明。维护代码可读性，多使用注释。


[RELY]

请先阅读：
- code/kern/mm/slub.h
- code/kern/mm/slub.c
- code/kern/mm/pmm.h
- code/kern/init/init.c

可使用工程现有的基础类型、页面大小定义、内存操作、assert 和 cprintf，相关定义位于：
- code/libs/defs.h
- code/kern/mm/mmu.h
- code/libs/string.h
- code/kern/debug/assert.h
- code/libs/stdio.h

主要接口：

```c
void *slub_alloc(size_t size);
void slub_free(void *ptr);
size_t nr_free_pages(void);
```

当前约定：
- 小对象类别为 16、32、64、128、256、512、1024、2048 字节，
  较大对象通过连续物理页分配。
- 返回地址至少满足 16 字节对齐，数据不保证初始为零。
- slub_alloc(0) 返回 NULL，slub_free(NULL) 不进行操作。
- 空 slab 立即归还物理页，大对象释放时归还其全部物理页。
- kern_init() 已调用 slub_init()，测试不得再次初始化 SLUB。
- 测试开始时没有存活的 SLUB 对象，也没有其他并发分配者。


[GUARANTEE]

必须实现并在测试头文件中声明：

```c
void slub_check(void);
```

必须修改现有函数：

```c
int kern_init(void);
```

接入要求：保留已有初始化流程，在 slub_init() 之后调用 slub_check()。

可以添加模块内部的 static 辅助函数和测试记录，不访问或复制分配器的私有管理结构。


[SPECIFICATION]

## slub_check

**Pre-Condition**：
- 物理页分配器和 SLUB 已初始化。
- 当前没有存活的 SLUB 对象。

**Post-Condition**：
- 所有检查成功后返回，测试申请的对象全部释放。
- 结束时 nr_free_pages() 恢复到测试入口记录的值。
- 任一检查失败时通过断言定位，不输出整体通过标记。

  **Case 1：零长度与空指针**
  - 验证零长度申请返回 NULL，释放 NULL 不改变空闲页数。
  - 随后正常分配和释放仍可进行。

  **Case 2：大小边界与大对象**
  - 覆盖最小正长度、各大小类别边界前后的请求，
    以及超过 2048 字节和需要跨页的请求，例如 5000 字节。
  - 验证返回值非空、地址对齐，并能完整读写请求长度内的数据。

  **Case 3：对象之间的数据隔离**
  - 同时申请多个相同大小和不同大小的对象，检查有效区域不重叠。
  - 写入可区分的数据，在继续分配、释放其他对象后重新校验，
    确认存活对象的数据未被破坏。

  **Case 4：跨 slab 分配与对象复用**
  - 对同一类别进行有界批量分配，确保使用不止一个 slab。
  - 保留部分对象，释放另一部分，再申请同类对象。
  - 验证已有空位能够复用，存活对象保持有效；
    对应空位足够时，不应额外消耗物理页。
  - 不要求返回某个固定地址。

  **Case 5：物理页回收**
  - 混合申请、释放小对象和大对象，验证部分释放不影响其他对象。
  - 每组对象全部释放后，空闲页数恢复到该组开始时的值。
  - 基准值动态记录，不写死空闲页数、私有管理头大小或 slab 容量。

  **Case 6：分配失败与长度溢出**
  - 构造计算本身不溢出、但超过当前空闲页容量的大对象请求，
    验证返回 NULL。
  - 使用接近 size_t 上限的请求，验证长度溢出处理。
  - 失败前后空闲页数不变，已有对象的数据保持正确，
    随后仍能正常分配和释放。

**Requirements**：
- 只读写各对象请求长度内的空间，只释放合法且尚未释放的原始指针。
- 测试规模有界，避免大数组导致内核栈溢出；
  辅助记录不依赖被测 SLUB 分配器。
- 每组检查成功后输出对应的 PASS 标记。
- 仅在全部检查完成后输出：
  [SLUB] all tests passed!


## kern_init

**Pre-Condition**：
- 内核正在执行启动初始化。

**Post-Condition**：
- 保持 pmm_init()、slub_init()、slub_check() 的调用顺序。
- 保留原有物理页检查、启动输出和主循环。

````

### 三、实现迭代过程

模块与测试通过两份提示词分别生成。编译及相关测试顺利通过，没有额外修正轮次，结果证明见 [SLUB 测试截图](./images/SLUB分配算法测试通过截图.png)。
