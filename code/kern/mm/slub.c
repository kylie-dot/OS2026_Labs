#include <slub.h>
#include <pmm.h>
#include <mmu.h>
#include <list.h>
#include <string.h>
#include <assert.h>

// lab2 2413497：下层申请连续物理页，上层将单页 slab 划分为固定大小对象。
#define SLUB_ALIGNMENT 16

struct slub_cache {
    size_t object_size;
    list_entry_t slabs;
};

// 空闲对象的首字存放后继指针；分配后整个槽位均可供调用者使用。
struct slub_free_object {
    struct slub_free_object *next;
};

// 管理信息保存在首页开头，不使用或修改公共 Page 结构。
// cache 为 NULL 表示大对象，此时 page_count 记录完整的连续页数量。
struct slub_slab {
    list_entry_t link;
    struct Page *page;
    struct slub_cache *cache;
    struct slub_free_object *free_objects;
    size_t inuse;
    size_t page_count;
};

#define SLUB_HEADER_SIZE ROUNDUP(sizeof(struct slub_slab), SLUB_ALIGNMENT)

static const size_t slub_sizes[] = {16, 32, 64, 128, 256, 512, 1024, 2048};
#define SLUB_CACHE_COUNT (sizeof(slub_sizes) / sizeof(slub_sizes[0]))

static struct slub_cache slub_caches[SLUB_CACHE_COUNT];
static bool slub_initialized;

// 本工程的 KADDR 未启用，使用现有的物理地址转换和直接映射偏移。
static struct slub_slab *
slub_page_address(struct Page *page) {
    return (struct slub_slab *)(page2pa(page) + va_pa_offset);
}

static struct slub_slab *
slub_new_slab(struct slub_cache *cache) {
    struct Page *page = alloc_pages(1);
    if (page == NULL) {
        return NULL;
    }

    struct slub_slab *slab = slub_page_address(page);
    memset(slab, 0, SLUB_HEADER_SIZE);
    slab->page = page;
    slab->cache = cache;
    slab->page_count = 1;

    // 头部向上对齐，槽位大小均为 16 的倍数，保证每个对象对齐。
    size_t capacity = (PGSIZE - SLUB_HEADER_SIZE) / cache->object_size;
    char *objects = (char *)slab + SLUB_HEADER_SIZE;
    for (size_t i = 0; i < capacity; i ++) {
        struct slub_free_object *object =
            (struct slub_free_object *)(objects + i * cache->object_size);
        object->next = slab->free_objects;
        slab->free_objects = object;
    }
    // 构造完成后才挂链；申请失败时不会改变任何 cache。
    list_add(&cache->slabs, &slab->link);
    return slab;
}

void
slub_init(void) {
    assert(!slub_initialized);
    for (size_t i = 0; i < SLUB_CACHE_COUNT; i ++) {
        slub_caches[i].object_size = slub_sizes[i];
        list_init(&slub_caches[i].slabs);
    }
    slub_initialized = 1;
}

void *
slub_alloc(size_t size) {
    assert(slub_initialized);
    if (size == 0) {
        return NULL;
    }

    for (size_t i = 0; i < SLUB_CACHE_COUNT; i ++) {
        struct slub_cache *cache = &slub_caches[i];
        if (size > cache->object_size) {
            continue;
        }

        struct slub_slab *slab = NULL;
        list_entry_t *le = &cache->slabs;
        while ((le = list_next(le)) != &cache->slabs) {
            struct slub_slab *candidate = to_struct(le, struct slub_slab, link);
            if (candidate->free_objects != NULL) {
                slab = candidate;
                break;
            }
        }
        if (slab == NULL) {
            slab = slub_new_slab(cache);
            if (slab == NULL) {
                return NULL;
            }
        }

        struct slub_free_object *object = slab->free_objects;
        slab->free_objects = object->next;
        slab->inuse ++;
        return object;
    }

    // 大对象同样在首页预留头部，返回地址仍处于首页，便于释放时定位。
    // 先检查加法溢出，再用除法及余数向上取整，避免 ROUNDUP 的加法溢出。
    if (size > (size_t)-1 - SLUB_HEADER_SIZE) {
        return NULL;
    }
    size_t total = size + SLUB_HEADER_SIZE;
    size_t page_count = total / PGSIZE + (total % PGSIZE != 0);
    struct Page *page = alloc_pages(page_count);
    if (page == NULL) {
        return NULL;
    }

    struct slub_slab *slab = slub_page_address(page);
    memset(slab, 0, SLUB_HEADER_SIZE);
    slab->page = page;
    slab->page_count = page_count;
    return (char *)slab + SLUB_HEADER_SIZE;
}

void
slub_free(void *ptr) {
    assert(slub_initialized);
    if (ptr == NULL) {
        return;
    }

    // 小对象槽位和大对象的原始返回地址都位于所属分配的首页。
    struct slub_slab *slab =
        (struct slub_slab *)ROUNDDOWN((uintptr_t)ptr, PGSIZE);
    if (slab->cache == NULL) {
        free_pages(slab->page, slab->page_count);
        return;
    }

    assert(slab->inuse > 0);
    struct slub_free_object *object = ptr;
    object->next = slab->free_objects;
    slab->free_objects = object;
    slab->inuse --;
    if (slab->inuse == 0) {
        // 先摘链再归还物理页，cache 不保留指向已回收页面的节点。
        list_del(&slab->link);
        free_pages(slab->page, 1);
    }
}
