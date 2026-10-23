#ifndef __KERN_MM_BUDDY_PMM_H__
#define __KERN_MM_BUDDY_PMM_H__

#include <pmm.h>

// lab2 2411333: 伙伴系统物理页分配器，make BUDDY=1（USE_BUDDY_PMM）时启用。
extern const struct pmm_manager buddy_pmm_manager;

#endif /* !__KERN_MM_BUDDY_PMM_H__ */
