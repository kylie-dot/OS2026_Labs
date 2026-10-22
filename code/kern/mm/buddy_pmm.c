#include <pmm.h>
#include <list.h>
#include <buddy_pmm.h>
#include <stdio.h>

/* lab2 2411333: 伙伴系统(buddy system)物理页分配器。
 *
 * 设计约定：
 * - 全部按绝对物理页号(ppn)计算对齐与伙伴：伙伴页号 = 起始页号 ^ (1 << order)。
 *   可管理物理内存为 [DRAM_BASE, PHYSICAL_MEMORY_END)，共 0x8000 = 2^15 页，
 *   因此任何合法块不超过 2^15 页，阶数取 0..BUDDY_MAX_ORDER。
 * - 每阶一条空闲链表，只登记块首页；空闲块首页满足：
 *     PG_property=1，property=块大小(2^order)，其余页 flags/property/ref 全 0。
 * - 分配块首页 property 打包记录 (原请求页数 << 16) | 块大小(2^order)，
 *   两个分量都不超过 2^15=0x8000，可完整存入 unsigned int；
 *   释放时据此核验起点与请求量，先拒绝非法释放再改动链表和计数。
 * - 元数据全部复用 struct Page 现有字段，不使用 malloc，
 *   也不依赖尚未初始化的其他分配器。
 */

#define BUDDY_MAX_ORDER 15

/* 编译期核对：最大阶必须覆盖整个可管理物理内存 */
#if (PHYSICAL_MEMORY_END - DRAM_BASE) / PGSIZE > (1 << BUDDY_MAX_ORDER)
#error "BUDDY_MAX_ORDER cannot cover the managed physical memory"
#endif

/* 分配记录编解码：低 16 位是实际块大小 2^order，高 16 位是原请求页数 */
#define BUDDY_REC_PACK(req, size) \
    ((((unsigned int)(req)) << 16) | ((unsigned int)(size)))
#define BUDDY_REC_SIZE(rec) ((size_t)((rec) & 0xFFFFu))
#define BUDDY_REC_REQ(rec) ((size_t)((rec) >> 16))

/* 每阶一条空闲链表；空闲页总数单独计数 */
static list_entry_t buddy_free_list[BUDDY_MAX_ORDER + 1];
static size_t buddy_nr_free;

static void
buddy_init(void) {
    for (size_t k = 0; k <= BUDDY_MAX_ORDER; k++) {
        list_init(&buddy_free_list[k]);
    }
    buddy_nr_free = 0;
}

/* 将 head 起、阶为 order 的空闲块尽可能与同阶空闲伙伴逐级合并，
 * 结果插入对应阶链表。不修改 buddy_nr_free，由调用者统一记账。 */
static void
buddy_merge_and_insert(struct Page *head, size_t order) {
    size_t size = (size_t)1 << order;
    ppn_t cur = page2ppn(head);
    while (order < BUDDY_MAX_ORDER) {
        /* 伙伴物理页号 = 当前起始物理页号 ^ (1 << order) */
        ppn_t bpn = cur ^ ((ppn_t)1 << order);
        /* 候选伙伴块必须整体位于描述符覆盖的页号范围内，才允许访问其首页 */
        if (bpn < nbase || bpn + size > npage) {
            break;
        }
        struct Page *buddy = pages + (bpn - nbase);
        /* 伙伴须是已登记(非保留)、同阶且完整的空闲块，才可合并 */
        if (PageReserved(buddy) || !PageProperty(buddy) ||
            buddy->property != size) {
            break;
        }
        list_del_init(&(buddy->page_link));
        ClearPageProperty(buddy);
        buddy->property = 0;
        if (buddy < head) { /* 以较低物理页号作为合并后的新起点 */
            head = buddy;
            cur = bpn;
        }
        order++;
        size <<= 1;
    }
    head->property = (unsigned int)size;
    SetPageProperty(head);
    list_add(&buddy_free_list[order], &(head->page_link));
}

static void
buddy_init_memmap(struct Page *base, size_t n) {
    assert(n > 0);
    /* 交接契约：框架交给管理器的页必须全部处于保留状态 */
    for (struct Page *p = base; p != base + n; p++) {
        assert(PageReserved(p));
        p->flags = p->property = 0;
        set_page_ref(p, 0);
    }

    /* 从起点贪心切分：每步取同时满足物理页号对齐和剩余长度的最大合法块。
     * 每步至少推进 1 页，不越界、不丢弃非 2 的幂区域的尾页。 */
    ppn_t start = page2ppn(base);
    ppn_t cur = start;
    ppn_t end = start + (ppn_t)n;
    while (cur < end) {
        size_t rem = (size_t)(end - cur);
        size_t order = 0;
        while (order < BUDDY_MAX_ORDER &&
               ((size_t)1 << (order + 1)) <= rem &&
               (cur & (((ppn_t)1 << (order + 1)) - 1)) == 0) {
            order++;
        }
        /* 多次登记相邻区域时，可与既有同阶空闲伙伴合并；
         * 保留/未登记页带 PG_reserved，不会成为合并候选。 */
        buddy_merge_and_insert(base + (size_t)(cur - start), order);
        cur += (ppn_t)1 << order;
    }
    buddy_nr_free += n;
}

static struct Page *
buddy_alloc_pages(size_t n) {
    assert(n > 0);
    /* 请求超过支持的最大阶，向上取整后无法安全表示 */
    if (n > ((size_t)1 << BUDDY_MAX_ORDER)) {
        return NULL;
    }
    /* 总空闲数不足必失败；总量足够也不代表存在足够连续块，仍需逐阶查找 */
    if (n > buddy_nr_free) {
        return NULL;
    }
    /* 求满足 2^order >= n 的最小阶数 */
    size_t order = 0;
    while (((size_t)1 << order) < n) {
        order++;
    }
    /* 从目标阶向更高阶找第一个非空链表 */
    size_t j = order;
    while (j <= BUDDY_MAX_ORDER && list_empty(&buddy_free_list[j])) {
        j++;
    }
    if (j > BUDDY_MAX_ORDER) {
        return NULL; /* 碎片化：无足够连续块，链表与计数保持不变 */
    }
    struct Page *page = le2page(list_next(&buddy_free_list[j]), page_link);
    list_del_init(&(page->page_link));
    ClearPageProperty(page);
    /* 逐级对半拆分：高半部分作为新的低阶空闲块放回链表 */
    while (j > order) {
        j--;
        struct Page *half = page + ((size_t)1 << j);
        half->property = (unsigned int)((size_t)1 << j);
        SetPageProperty(half);
        list_add(&buddy_free_list[j], &(half->page_link));
    }
    /* 记录原请求与实际块大小，供释放时核验 */
    page->property = BUDDY_REC_PACK(n, (size_t)1 << order);
    buddy_nr_free -= (size_t)1 << order;
    return page;
}

static void
buddy_free_pages(struct Page *base, size_t n) {
    assert(n > 0);
    assert(!PageReserved(base) && !PageProperty(base));

    /* 释放前核验原分配记录：重复释放(已成空闲块首页/内部页)、错误起点、
     * 请求量不匹配等都在改动任何链表或计数之前被拒绝。 */
    unsigned int rec = base->property;
    size_t size = BUDDY_REC_SIZE(rec);
    assert(BUDDY_REC_REQ(rec) == n);      /* 请求量必须等于原申请 */
    assert(size >= n && n > (size >> 1)); /* 块大小与分配时同样地向上取整 */
    assert((size & (size - 1)) == 0);     /* 块大小是 2 的幂 */
    assert(size <= ((size_t)1 << BUDDY_MAX_ORDER));
    ppn_t ppn = page2ppn(base);
    assert(ppn >= nbase && ppn + size <= npage); /* 整块位于管理范围内 */
    assert((ppn & (size - 1)) == 0);              /* 按绝对物理页号对齐 */

    size_t order = 0;
    while (((size_t)1 << order) < size) {
        order++;
    }

    /* 整块清回干净状态 */
    for (struct Page *p = base; p != base + size; p++) {
        assert(!PageReserved(p) && !PageProperty(p));
        p->flags = 0;
        p->property = 0;
        set_page_ref(p, 0);
    }
    /* 空闲数只按实际归还的整块增加一次，后续合并不再重复计数 */
    buddy_nr_free += size;
    buddy_merge_and_insert(base, order);
}

static size_t
buddy_nr_free_pages(void) {
    return buddy_nr_free;
}

/* 走查全部阶的空闲链表，核对基础不变量；返回统计出的空闲页总数。
 * 逐块检查大小、对齐、范围和标志；逐页检查内部页可同时发现空闲块互相重叠。 */
static size_t
buddy_check_walk(void) {
    size_t total = 0;
    for (size_t k = 0; k <= BUDDY_MAX_ORDER; k++) {
        size_t size = (size_t)1 << k;
        list_entry_t *le = &buddy_free_list[k];
        for (le = list_next(le); le != &buddy_free_list[k]; le = list_next(le)) {
            struct Page *p = le2page(le, page_link);
            assert(PageProperty(p));
            assert(p->property == size);
            ppn_t ppn = page2ppn(p);
            assert(ppn >= nbase && ppn + size <= npage);
            assert((ppn & (size - 1)) == 0);
            assert(!PageReserved(p));
            for (size_t i = 1; i < size; i++) {
                struct Page *q = p + i;
                assert(!PageReserved(q) && !PageProperty(q) &&
                       q->property == 0 && page_ref(q) == 0);
            }
            total += size;
        }
    }
    return total;
}

/* ===================== lab2 2411333: 独立针对性测试 =====================
 *
 * 隔离方案：固定 16/19 页的情景不能直接用整机空闲池验证（其余内存会掩盖
 * 预期失败），因此在模块内建立有界 static 模拟描述符数组，仅测试描述符层
 * 算法。模拟期间临时把 pages 指向模拟数组、npage 设为 nbase+模拟页数；
 * nbase 是 const，只读不改；不改 va_pa_offset。
 *
 * 状态保存与恢复的完整性依据：模块的全部可变共享状态只有 pages、npage、
 * 各阶链表头哨兵和空闲计数。真实空闲块节点（真实 Page 的 page_link）在
 * 模拟期间不会被触及——链表操作只经过重新初始化的哨兵和模拟描述符。
 * 因此进入前整体保存这四项，结束后原样拷回：哨兵中保存的 prev/next 仍
 * 指向从未被改动的真实节点，链表即完整恢复。复制出的真实哨兵仅作为恢复
 * 快照，模拟期间不当作可遍历链表使用。
 *
 * 模拟页只是测试描述符：page2ppn/page2pa 的计算值仅用于对齐与顺序判断，
 * 绝不用于读写真实物理内存。所有期望值按规格独立推导（绝对页号对齐、
 * 贪心切分、向上取整、异或伙伴），不复制实现行为作为预期。
 */

#define BUDDY_TEST_PAGES 32 /* 规格要求测试数组至少 32 个描述符 */

/* 模拟描述符数组：.bss 中的有界静态存储，不占用任何分配器页面 */
static struct Page buddy_test_pages[BUDDY_TEST_PAGES];

/* 真实模块环境的完整快照 */
struct buddy_test_env {
    struct Page *pages;
    size_t npage;
    list_entry_t lists[BUDDY_MAX_ORDER + 1];
    size_t nr_free;
};

static struct buddy_test_env buddy_test_env_saved;

/* 失败副作用的完整快照（lab2 2411333）。
 * 注意：各阶哨兵 prev/next 相同只能证明链表首尾节点指向未变，
 * 并不能证明链表内部节点或页描述符未被改动；空闲页计数也只是
 * 汇总量。因此快照额外逐字段保存全部模拟页描述符的 flags、
 * property、ref 及其 page_link 的 prev/next，比较时逐项比对，
 * 全部一致才判定失败无副作用。不用整块内存比较，避免把结构体
 * 填充字节误判为差异。 */
struct buddy_test_page_snap {
    uint64_t flags;       /* 模拟页标志（含 PG_reserved/PG_property） */
    unsigned int property; /* 模拟页块元数据 */
    int ref;              /* 模拟页引用计数 */
    list_entry_t link;    /* 模拟页 page_link 的 prev/next 拷贝 */
};

struct buddy_test_state {
    list_entry_t lists[BUDDY_MAX_ORDER + 1]; /* 各阶链表头哨兵 */
    size_t nr_free;                          /* 空闲页总数 */
    struct buddy_test_page_snap pages[BUDDY_TEST_PAGES]; /* 全部模拟页 */
};

/* 有界静态快照：save/assert_same 严格成对顺序使用、不嵌套，
 * 单实例即可；避免在内核栈上放置 ~1.3KB 的快照。 */
static struct buddy_test_state buddy_test_state_snapshot;

static void
buddy_test_state_save(void) {
    for (size_t k = 0; k <= BUDDY_MAX_ORDER; k++) {
        buddy_test_state_snapshot.lists[k] = buddy_free_list[k];
    }
    buddy_test_state_snapshot.nr_free = buddy_nr_free;
    for (size_t i = 0; i < BUDDY_TEST_PAGES; i++) {
        struct Page *p = buddy_test_pages + i;
        buddy_test_state_snapshot.pages[i].flags = p->flags;
        buddy_test_state_snapshot.pages[i].property = p->property;
        buddy_test_state_snapshot.pages[i].ref = p->ref;
        buddy_test_state_snapshot.pages[i].link = p->page_link;
    }
}

/* 逐项比对快照：计数、各阶哨兵 prev/next、全部模拟页的
 * flags/property/ref 及 page_link 的 prev/next。任一项不一致
 * 即断言失败。 */
static void
buddy_test_state_assert_same(void) {
    assert(buddy_nr_free == buddy_test_state_snapshot.nr_free);
    for (size_t k = 0; k <= BUDDY_MAX_ORDER; k++) {
        assert(buddy_free_list[k].prev == buddy_test_state_snapshot.lists[k].prev &&
               buddy_free_list[k].next == buddy_test_state_snapshot.lists[k].next);
    }
    for (size_t i = 0; i < BUDDY_TEST_PAGES; i++) {
        struct Page *p = buddy_test_pages + i;
        assert(p->flags == buddy_test_state_snapshot.pages[i].flags);
        assert(p->property == buddy_test_state_snapshot.pages[i].property);
        assert(p->ref == buddy_test_state_snapshot.pages[i].ref);
        assert(p->page_link.prev == buddy_test_state_snapshot.pages[i].link.prev &&
               p->page_link.next == buddy_test_state_snapshot.pages[i].link.next);
    }
}

/* 保存真实环境；必须在任何 buddy_test_env_reset 之前调用一次 */
static void
buddy_test_env_save(void) {
    buddy_test_env_saved.pages = pages;
    buddy_test_env_saved.npage = npage;
    for (size_t k = 0; k <= BUDDY_MAX_ORDER; k++) {
        buddy_test_env_saved.lists[k] = buddy_free_list[k];
    }
    buddy_test_env_saved.nr_free = buddy_nr_free;
}

/* 恢复真实环境：把四项共享状态原样写回 */
static void
buddy_test_env_restore(void) {
    pages = buddy_test_env_saved.pages;
    npage = buddy_test_env_saved.npage;
    for (size_t k = 0; k <= BUDDY_MAX_ORDER; k++) {
        buddy_free_list[k] = buddy_test_env_saved.lists[k];
    }
    buddy_nr_free = buddy_test_env_saved.nr_free;
}

/* 建立模拟环境：每组测试开始前独立初始化。
 * 前提：nbase 本身 16 页对齐（DRAM_BASE=0x80000000，nbase=0x80000），
 * 这样模拟数组内相对偏移的对齐性等价于绝对页号的对齐性（块 <= 16 页）。 */
static void
buddy_test_env_reset(size_t npages) {
    assert(npages > 0 && npages <= BUDDY_TEST_PAGES);
    assert((nbase & 15) == 0); /* 前提不满足则本组期望全部失效 */
    pages = buddy_test_pages;
    npage = nbase + npages;
    buddy_init(); /* 清空各阶链表与空闲计数 */
    /* 全部模拟页重置为干净的保留页；未登记页始终保持 Reserved */
    for (size_t i = 0; i < BUDDY_TEST_PAGES; i++) {
        struct Page *p = buddy_test_pages + i;
        p->flags = 0;
        SetPageReserved(p);
        p->property = 0;
        set_page_ref(p, 0);
        list_init(&(p->page_link));
    }
}

/* 统计 order 阶链表中的块数 */
static size_t
buddy_test_order_blocks(size_t order) {
    size_t n = 0;
    list_entry_t *le = &buddy_free_list[order];
    for (le = list_next(le); le != &buddy_free_list[order]; le = list_next(le)) {
        n++;
    }
    return n;
}

/* 判断 head 是否为 order 阶链表中的块首页 */
static bool
buddy_test_in_order_list(size_t order, struct Page *head) {
    list_entry_t *le = &buddy_free_list[order];
    for (le = list_next(le); le != &buddy_free_list[order]; le = list_next(le)) {
        if (le2page(le, page_link) == head) {
            return 1;
        }
    }
    return 0;
}

/* 组1：区域登记与贪心切分（lab2 2411333）。
 * 依据：order=k 块须绝对页号对齐 2^k；贪心切分从起点取最大合法块，
 * 16 页对齐起点登记 19 页应为 16+2+1 且尾页可用；未登记页保持 Reserved；
 * 候选伙伴越界（npage 截断）或未登记（Reserved）时不得误合并。
 * 局限：仅覆盖 <=19 页、单区域/两相邻区域的描述符层行为，
 * 不涉及真实物理内存与多区域混合布局。 */
static void
buddy_test_init_ranges(void) {
    /* (a) 16 页对齐起点登记 19 页：16 + 2 + 1，尾页可用。
     * npage 取 nbase+19：三个块的候选伙伴均落在管理范围之外，
     * 用于检验越界保护路径（不得误合并、不得越界访问）。 */
    buddy_test_env_reset(19);
    struct Page *base = buddy_test_pages + 0; /* 绝对页号 nbase，16 对齐 */
    assert((page2ppn(base) & 15) == 0);
    buddy_init_memmap(base, 19);
    assert(buddy_nr_free == 19);
    assert(buddy_test_order_blocks(4) == 1);
    assert(buddy_test_order_blocks(1) == 1);
    assert(buddy_test_order_blocks(0) == 1);
    assert(buddy_test_order_blocks(2) == 0 && buddy_test_order_blocks(3) == 0);
    /* 16 页块在起点；2 页块在偏移 16；1 页块（尾页）在偏移 18 */
    assert(PageProperty(base) && base->property == 16);
    assert(buddy_test_in_order_list(4, base));
    struct Page *b2 = buddy_test_pages + 16;
    struct Page *b1 = buddy_test_pages + 18;
    assert(PageProperty(b2) && b2->property == 2);
    assert(buddy_test_in_order_list(1, b2));
    assert(PageProperty(b1) && b1->property == 1);
    assert(buddy_test_in_order_list(0, b1));
    for (size_t i = 0; i < 19; i++) { /* 登记页全部解除保留 */
        assert(!PageReserved(buddy_test_pages + i));
    }
    /* 尾页可用：order 0 链表仅有尾页一块，申请 1 页必得尾页 */
    struct Page *tail = buddy_alloc_pages(1);
    assert(tail == b1);
    assert(buddy_nr_free == 18);
    buddy_free_pages(tail, 1);
    assert(buddy_nr_free == 19);

    /* (b) 重建空环境，只登记偏移 4..15（12 页）：
     * 4..7 为 order 2，8..15 为 order 3，其余页不进链表且保持 Reserved。 */
    buddy_test_env_reset(32);
    base = buddy_test_pages + 4;
    buddy_init_memmap(base, 12);
    assert(buddy_nr_free == 12);
    assert(buddy_test_order_blocks(2) == 1 && buddy_test_order_blocks(3) == 1);
    assert(buddy_test_order_blocks(0) == 0 && buddy_test_order_blocks(1) == 0);
    assert(PageProperty(buddy_test_pages + 4) && buddy_test_pages[4].property == 4);
    assert(buddy_test_in_order_list(2, buddy_test_pages + 4));
    assert(PageProperty(buddy_test_pages + 8) && buddy_test_pages[8].property == 8);
    assert(buddy_test_in_order_list(3, buddy_test_pages + 8));
    for (size_t i = 0; i < 4; i++) { /* 范围内未登记页保持 Reserved */
        assert(PageReserved(buddy_test_pages + i));
    }
    for (size_t i = 16; i < BUDDY_TEST_PAGES; i++) {
        assert(PageReserved(buddy_test_pages + i));
    }
    /* 偏移 4 的 order 2 块的候选伙伴在偏移 0（未登记）：
     * 未误合并已由 order 2 计数恒为 1 证明。 */

    /* (c) 分两次登记相邻伙伴区域 0..3、4..7：应合并为 order 3，
     * 可申请完整 8 页。 */
    buddy_test_env_reset(32);
    buddy_init_memmap(buddy_test_pages + 0, 4);
    assert(buddy_nr_free == 4);
    assert(buddy_test_order_blocks(2) == 1);
    assert(PageReserved(buddy_test_pages + 4)); /* 第二区域仍为保留页 */
    buddy_init_memmap(buddy_test_pages + 4, 4);
    assert(buddy_nr_free == 8);
    assert(buddy_test_order_blocks(2) == 0); /* 已合并 */
    assert(buddy_test_order_blocks(3) == 1);
    assert(PageProperty(buddy_test_pages + 0) && buddy_test_pages[0].property == 8);
    struct Page *whole = buddy_alloc_pages(8);
    assert(whole == buddy_test_pages + 0); /* order 3 链表仅有这一块 */
    assert((page2ppn(whole) & 7) == 0);
    assert(buddy_nr_free == 0);
    buddy_free_pages(whole, 8);
    assert(buddy_nr_free == 8);

    /* (d) 单页范围尾部：登记最后一页，候选伙伴（倒数第二页）未登记，
     * 不误合并、不越界。 */
    buddy_test_env_reset(32);
    buddy_init_memmap(buddy_test_pages + 31, 1);
    assert(buddy_nr_free == 1);
    assert(buddy_test_order_blocks(0) == 1);
    assert(buddy_test_in_order_list(0, buddy_test_pages + 31));
    for (size_t i = 0; i < 31; i++) {
        assert(PageReserved(buddy_test_pages + i));
    }
    struct Page *only = buddy_alloc_pages(1);
    assert(only == buddy_test_pages + 31); /* order 0 链表仅有这一块 */
    buddy_free_pages(only, 1);
    assert(buddy_nr_free == 1);

    cprintf("[BUDDY] 1 init ranges: PASS\n");
}

/* 组2：取整、拆分与合并（lab2 2411333）。
 * 依据：alloc(n) 取最小阶 2^order>=n 并按实际块大小扣减；拆分时高半部分
 * 回填低阶链表；释放按原请求归还整块并逐级合并。位置断言均有推导依据
 * （每步相关阶链表恰有一块），不依赖返回顺序假设。
 * 局限：单一 16 页对齐区域，未覆盖高阶、多区域混合拆分。 */
static void
buddy_test_split_merge(void) {
    buddy_test_env_reset(32);
    struct Page *base = buddy_test_pages + 0; /* 16 页对齐区域 [0,16) */
    assert((page2ppn(base) & 15) == 0);
    buddy_init_memmap(base, 16);
    assert(buddy_nr_free == 16);
    assert(buddy_test_order_blocks(4) == 1);

    /* 申请 3 页：实际扣除 4 页，返回块按 4 页对齐 */
    struct Page *a = buddy_alloc_pages(3);
    assert(a != NULL);
    assert((page2ppn(a) & 3) == 0);
    assert(buddy_nr_free == 12);
    assert(!PageProperty(a));
    assert(BUDDY_REC_REQ(a->property) == 3 && BUDDY_REC_SIZE(a->property) == 4);

    /* 申请 5 页：实际扣除 8 页，返回块按 8 页对齐 */
    struct Page *b = buddy_alloc_pages(5);
    assert(b != NULL);
    assert((page2ppn(b) & 7) == 0);
    assert(buddy_nr_free == 4);
    assert(!PageProperty(b));
    assert(BUDDY_REC_REQ(b->property) == 5 && BUDDY_REC_SIZE(b->property) == 8);

    /* 两个实际分配区间不重叠；剩余恰为一个 4 页空闲块 */
    ppn_t pa = page2ppn(a), pb = page2ppn(b);
    assert(pa + 4 <= pb || pb + 8 <= pa);
    assert(buddy_test_order_blocks(2) == 1);
    assert(buddy_nr_free == 4);

    /* 按原请求量释放两块，空闲数恢复 16 */
    buddy_free_pages(a, 3);
    assert(buddy_nr_free == 8);
    buddy_free_pages(b, 5);
    assert(buddy_nr_free == 16);

    /* 申请 16 页成功证明确实递归合并为整块（不能只看计数） */
    struct Page *c = buddy_alloc_pages(16);
    assert(c != NULL);
    assert((page2ppn(c) & 15) == 0);
    assert(buddy_nr_free == 0);
    buddy_free_pages(c, 16);
    assert(buddy_nr_free == 16);

    /* 归还后完整恢复：仅 order 4 一个 16 页块，位于区域起点
     * （16 页区域内唯一可能的 16 页块就是整个区域）。 */
    assert(buddy_test_order_blocks(4) == 1);
    /* 其余各阶空闲块数均为 0：遍历 0 至 BUDDY_MAX_ORDER（含最大阶），
     * k == 4 的情形已由上一断言单独覆盖，不重复检查。 */
    for (size_t k = 0; k <= BUDDY_MAX_ORDER; k++) {
        if (k != 4) {
            assert(buddy_test_order_blocks(k) == 0);
        }
    }
    assert(PageProperty(base) && base->property == 16);
    assert(buddy_test_in_order_list(4, base));

    cprintf("[BUDDY] 2 split/merge: PASS\n");
}

/* 组3：碎片化失败与状态恢复（lab2 2411333）。
 * 依据：空闲总量足够不代表存在连续块；失败不得改动链表、计数或存活
 * 分配记录；相邻释放应合并并重新满足大块请求。
 * 局限：4x4 页单一布局；未覆盖高阶碎片与多轮交错分配。 */
static void
buddy_test_fragmentation(void) {
    buddy_test_env_reset(32);
    buddy_init_memmap(buddy_test_pages + 0, 16);
    assert(buddy_nr_free == 16);

    /* 分配四块各 4 页，铺满整个 16 页区域 */
    struct Page *blk[4];
    for (size_t i = 0; i < 4; i++) {
        blk[i] = buddy_alloc_pages(4);
        assert(blk[i] != NULL);
        assert((page2ppn(blk[i]) & 3) == 0);
        assert(BUDDY_REC_REQ(blk[i]->property) == 4 &&
               BUDDY_REC_SIZE(blk[i]->property) == 4);
    }
    assert(buddy_nr_free == 0);

    /* 按物理页号排序确定实际空间顺序（ppn 序即物理地址序），
     * 不依赖分配返回顺序 */
    size_t ord[4] = {0, 1, 2, 3};
    for (size_t i = 0; i < 4; i++) {
        for (size_t j = i + 1; j < 4; j++) {
            if (page2ppn(blk[ord[j]]) < page2ppn(blk[ord[i]])) {
                size_t t = ord[i];
                ord[i] = ord[j];
                ord[j] = t;
            }
        }
    }
    /* 四个不重叠的 4 页块铺满 16 页区域：排序后必两两相邻 */
    for (size_t i = 0; i + 1 < 4; i++) {
        assert(page2ppn(blk[ord[i]]) + 4 == page2ppn(blk[ord[i + 1]]));
    }

    /* 释放空间序第 1、3 块：空闲 8 页但被分割为两个 4 页块 */
    buddy_free_pages(blk[ord[0]], 4);
    buddy_free_pages(blk[ord[2]], 4);
    assert(buddy_nr_free == 8);
    assert(buddy_test_order_blocks(2) == 2);

    buddy_test_state_save();
    /* 申请 8 页：总量足够但无连续 8 页，必须失败 */
    assert(buddy_alloc_pages(8) == NULL);
    /* 失败无副作用：计数、各阶哨兵、全部模拟页描述符（flags/property/
     * ref/page_link）逐项与失败前完全一致，不只比总数 */
    buddy_test_state_assert_same();
    /* 仍存活的分配记录保持有效 */
    assert(BUDDY_REC_REQ(blk[ord[1]]->property) == 4 &&
           BUDDY_REC_SIZE(blk[ord[1]]->property) == 4);
    assert(BUDDY_REC_REQ(blk[ord[3]]->property) == 4 &&
           BUDDY_REC_SIZE(blk[ord[3]]->property) == 4);
    assert(!PageProperty(blk[ord[1]]) && !PageReserved(blk[ord[1]]));
    assert(!PageProperty(blk[ord[3]]) && !PageReserved(blk[ord[3]]));

    /* 释放空间序第 2 块：与第 1 块合并成 8 页连续块 */
    buddy_free_pages(blk[ord[1]], 4);
    assert(buddy_nr_free == 12);
    assert(buddy_test_order_blocks(3) == 1);
    /* 申请 8 页成功，剩余空闲 4 页 */
    struct Page *e = buddy_alloc_pages(8);
    assert(e != NULL);
    assert((page2ppn(e) & 7) == 0);
    assert(buddy_nr_free == 4);

    /* 归还新块与仍存活的第 4 块，恢复完整 16 页；
     * 再次成功申请和归还 16 页证明完全合并。 */
    buddy_free_pages(e, 8);
    buddy_free_pages(blk[ord[3]], 4);
    assert(buddy_nr_free == 16);
    struct Page *f = buddy_alloc_pages(16);
    assert(f != NULL);
    assert((page2ppn(f) & 15) == 0);
    buddy_free_pages(f, 16);
    assert(buddy_nr_free == 16);

    cprintf("[BUDDY] 3 fragmentation: PASS\n");
}

/* 组4：失败路径状态保持（lab2 2411333）。
 * 依据：分配失败返回 NULL 且不得改动链表、计数或存活分配记录；
 * 超过支持阶数与接近 size_t 上限的请求必须安全失败。
 * 局限：零请求与非法释放按设计会 panic，不在启动路径动态触发。 */
static void
buddy_test_failure_state(void) {
    buddy_test_env_reset(32);
    buddy_init_memmap(buddy_test_pages + 0, 16);
    assert(buddy_nr_free == 16);

    buddy_test_state_save();
    /* 17 页超过登记总量；2^16 超过实现支持的最大阶；接近 size_t 上限 */
    assert(buddy_alloc_pages(17) == NULL);
    assert(buddy_alloc_pages((size_t)1 << 16) == NULL);
    assert(buddy_alloc_pages((size_t)-1) == NULL);
    assert(buddy_alloc_pages((size_t)-2) == NULL);
    /* 失败无副作用：计数、各阶哨兵、全部模拟页描述符逐项一致 */
    buddy_test_state_assert_same();

    /* 保留一个合法存活分配，再做超大请求 */
    struct Page *live = buddy_alloc_pages(5);
    assert(live != NULL);
    assert((page2ppn(live) & 7) == 0);
    assert(buddy_nr_free == 8);
    unsigned int live_rec = live->property;
    buddy_test_state_save();
    assert(buddy_alloc_pages(17) == NULL); /* 17 > 剩余 8 */
    assert(buddy_alloc_pages((size_t)-1) == NULL);
    buddy_test_state_assert_same();
    /* 存活分配记录未变，且仍可按原请求释放 */
    assert(live->property == live_rec);
    assert(!PageProperty(live) && !PageReserved(live));
    buddy_free_pages(live, 5);
    assert(buddy_nr_free == 16);

    /* 随后普通分配和释放仍成功 */
    struct Page *q = buddy_alloc_pages(1);
    assert(q != NULL);
    assert(buddy_nr_free == 15);
    buddy_free_pages(q, 1);
    assert(buddy_nr_free == 16);

    cprintf("[BUDDY] 4 failure state: PASS\n");
}

/* 基础检查 + 独立测试：不变量走查、受控模拟环境四组测试、
 * 恢复真实环境后的真实分配/释放一致性复查。 */
static void
buddy_check(void) {
    /* 本检查仅经 pmm_manager->check 触发，运行时管理器必须就是 Buddy */
    assert(pmm_manager == &buddy_pmm_manager);

    /* 入口快照：结束时必须完全恢复空闲数与分配能力 */
    size_t entry_free = nr_free_pages();
    assert(entry_free > 0);
    assert(buddy_check_walk() == entry_free);

    /* —— 独立针对性测试（lab2 2411333）：受控模拟环境 ——
     * 保存真实环境 -> 四组测试（每组自行重建模拟环境）-> 恢复真实环境。 */
    buddy_test_env_save();
    buddy_test_init_ranges();
    buddy_test_split_merge();
    buddy_test_fragmentation();
    buddy_test_failure_state();
    buddy_test_env_restore();

    /* —— 恢复真实环境后的基础一致性复查（原有检查全部保留） —— */

    /* 多种请求尺寸的回环：返回块对齐到 2^ceil(log2(n))，记账只按实际块大小 */
    static const size_t reqs[] = {1, 2, 3, 4, 5, 7, 8, 9, 16, 65};
    for (size_t i = 0; i < sizeof(reqs) / sizeof(reqs[0]); i++) {
        size_t n = reqs[i];
        size_t order = 0;
        while (((size_t)1 << order) < n) {
            order++;
        }
        size_t before = nr_free_pages();
        struct Page *p = alloc_pages(n);
        assert(p != NULL);
        assert(!PageProperty(p));
        assert((page2ppn(p) & (((size_t)1 << order) - 1)) == 0);
        assert(nr_free_pages() == before - ((size_t)1 << order));
        free_pages(p, n);
        assert(nr_free_pages() == before);
    }

    /* 大块路径：从大到小探测一个可整体分配的块并原样归还 */
    size_t big = 0;
    for (size_t sz = 2048; sz >= 64; sz >>= 1) {
        if (sz > entry_free) {
            continue;
        }
        struct Page *p = alloc_pages(sz);
        if (p == NULL) {
            continue;
        }
        assert((page2ppn(p) & (sz - 1)) == 0);
        free_pages(p, sz);
        big = sz;
        break;
    }
    assert(big > 0);
    assert(nr_free_pages() == entry_free);

    /* 超过支持阶数/总量的请求必须失败，且失败不改变任何状态 */
    size_t before = nr_free_pages();
    assert(alloc_pages((size_t)1 << 20) == NULL);
    assert(alloc_pages((size_t)-1) == NULL);
    assert(nr_free_pages() == before);

    /* 出口复查：不变量成立，入口空闲数恢复 */
    assert(nr_free_pages() == entry_free);
    assert(buddy_check_walk() == entry_free);

    cprintf("buddy_check(): basic invariants and alloc/free round-trip passed\n");
    /* 零请求与非法释放按设计以 assert/panic 拒绝，属于预期致命路径，
     * 不在正常启动中动态触发：这两类拒绝路径尚未独立动态验证。 */
    cprintf("[BUDDY] note: zero-request / illegal-free rejection paths panic "
            "by design and are NOT dynamically verified here\n");
    /* 四组独立测试已全部通过，真实环境已恢复并完成上述复查 */
    cprintf("[BUDDY] independent tests passed!\n");
}

const struct pmm_manager buddy_pmm_manager = {
    .name = "buddy_pmm_manager",
    .init = buddy_init,
    .init_memmap = buddy_init_memmap,
    .alloc_pages = buddy_alloc_pages,
    .free_pages = buddy_free_pages,
    .nr_free_pages = buddy_nr_free_pages,
    .check = buddy_check,
};
