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

（结构同上。手册要求两份 prompt：①参考现有 ucore 框架从零生成 Buddy 模块并接入内核 ②为该实现设计测试函数；报告需说明测试覆盖与有效性。）

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
