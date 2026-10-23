#ifndef __KERN_MM_SLUB_H__
#define __KERN_MM_SLUB_H__

#include <defs.h>

// lab2 2413497：基于物理页分配器的简化 SLUB 对象分配接口。
// 初始化不占用物理页，须在 pmm_init() 完成后调用一次。
void slub_init(void);

// 返回至少 size 字节、16 字节对齐的区域；零长度或失败时返回 NULL。
void *slub_alloc(size_t size);

// 释放尚未释放的原始指针；NULL 无操作，空 slab 立即归还物理页。
void slub_free(void *ptr);

#endif /* !__KERN_MM_SLUB_H__ */
