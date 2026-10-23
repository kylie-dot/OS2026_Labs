#include <default_pmm.h>
#include <best_fit_pmm.h>
#include <defs.h>
#include <error.h>
#include <memlayout.h>
#include <mmu.h>
#include <pmm.h>
#include <sbi.h>
#include <stdio.h>
#include <string.h>
#include <riscv.h>
#include <dtb.h>

// virtual address of physical page array
struct Page *pages;
// amount of physical memory (in pages)
size_t npage = 0;
// the kernel image is mapped at VA=KERNBASE and PA=info.base
uint64_t va_pa_offset;
// memory starts at 0x80000000 in RISC-V
// DRAM_BASE defined in riscv.h as 0x80000000
const size_t nbase = DRAM_BASE / PGSIZE;

// virtual address of boot-time page directory
uintptr_t *satp_virtual = NULL;
// physical address of boot-time page directory
uintptr_t satp_physical;

// physical memory management
const struct pmm_manager *pmm_manager;


static void check_alloc_page(void);

// init_pmm_manager - initialize a pmm_manager instance
static void init_pmm_manager(void) {
    pmm_manager = &default_pmm_manager;
    cprintf("memory management: %s\n", pmm_manager->name);
    pmm_manager->init();
}

// init_memmap - call pmm->init_memmap to build Page struct for free memory
static void init_memmap(struct Page *base, size_t n) {
    pmm_manager->init_memmap(base, n);
}

// alloc_pages - call pmm->alloc_pages to allocate a continuous n*PAGESIZE
// memory
struct Page *alloc_pages(size_t n) {
    return pmm_manager->alloc_pages(n);
}

// free_pages - call pmm->free_pages to free a continuous n*PAGESIZE memory
void free_pages(struct Page *base, size_t n) {
    pmm_manager->free_pages(base, n);
}

// nr_free_pages - call pmm->nr_free_pages to get the size (nr*PAGESIZE)
// of current free memory
size_t nr_free_pages(void) {
    return pmm_manager->nr_free_pages();
}

//lab2 function1 2411333
static void page_init(void){
    // kernel.ld 提供的链接符号：内核镜像（.bss）结束后的第一个地址
    extern char end[];

    // ① 读取设备树解析出的物理内存信息，无效则终止初始化
    uint64_t base = get_memory_base();
    uint64_t size = get_memory_size();
    if (base == 0 || size == 0) {
        panic("page_init: invalid memory info from dtb\n");
    }

    // ② 设置三个全局变量（必须先于下面所有用到 PADDR 的计算）
    // 内核镜像被放在 VA = KERNBASE、PA = KERNEL_BEGIN_PADDR 处，
    // 虚实偏移 = KERNBASE - KERNEL_BEGIN_PADDR，即 memlayout.h 的 PHYSICAL_MEMORY_OFFSET
    va_pa_offset = KERNBASE - KERNEL_BEGIN_PADDR;
    // 可管理内存上界：设备树报告的内存末端与物理内存上界取小
    uint64_t mem_end = base + size;
    if (mem_end > PHYSICAL_MEMORY_END) {
        mem_end = PHYSICAL_MEMORY_END;
    }
    npage = PPN(mem_end);
    // 页描述符数组紧跟内核镜像之后，向上取整到整页
    pages = (struct Page *)ROUNDUP((uintptr_t)end, PGSIZE);

    // ③ 把可管理范围内的全部页先标记为保留（default_init_memmap 的进门契约：
    // 交给它的每个页必须已是 PG_reserved）
    // 物理页号 [nbase, npage) 对应数组下标 [0, npage - nbase)，见 pa2page()
    for (size_t i = 0; i < npage - nbase; i++) {
        SetPageReserved(pages + i);
    }

    // ④ 空闲区 = 描述符数组末尾（换算成物理地址并向上取整到整页）之后的内存
    uintptr_t array_end = PADDR(pages + (npage - nbase));
    uintptr_t freemem = ROUNDUP(array_end, PGSIZE);
    size_t nfree = 0;
    if (PPN(freemem) < npage) {
        nfree = npage - PPN(freemem);
    }
    // 至少有一个完整空闲页，才把空闲区交给分配器
    if (nfree > 0) {
        init_memmap(pa2page(freemem), nfree);
    }

    // ⑤ 打印内存布局
    cprintf("memory: base = 0x%016lx, size = 0x%016lx\n", base, size);
    cprintf("free memory: start = 0x%016lx, pages = %lu\n", freemem, nfree);
}


/* pmm_init - initialize the physical memory management */
void pmm_init(void) {
    // We need to alloc/free the physical memory (granularity is 4KB or other size).
    // So a framework of physical memory manager (struct pmm_manager)is defined in pmm.h
    // First we should init a physical memory manager(pmm) based on the framework.
    // Then pmm can alloc/free the physical memory.
    // Now the first_fit/best_fit/worst_fit/buddy_system pmm are available.
    init_pmm_manager();

    // detect physical memory space, reserve already used memory,
    // then use pmm->init_memmap to create free page list
    page_init();

    // use pmm->check to verify the correctness of the alloc/free function in a pmm
    check_alloc_page();

    extern char boot_page_table_sv39[];
    satp_virtual = (pte_t*)boot_page_table_sv39;
    satp_physical = PADDR(satp_virtual);
    cprintf("satp virtual address: 0x%016lx\nsatp physical address: 0x%016lx\n", satp_virtual, satp_physical);
}

static void check_alloc_page(void) {
    pmm_manager->check();
    cprintf("check_alloc_page() succeeded!\n");
}
