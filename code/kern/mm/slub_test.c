#include <slub_test.h>
#include <slub.h>
#include <pmm.h>
#include <mmu.h>
#include <assert.h>
#include <stdio.h>

// lab2 2413497：只通过公开接口测试，不读取分配器的私有管理结构。
#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))
#define REUSE_SIZE 64
// 请求数据总量达到两页，保证单页 slab 无法容纳这批对象。
// 此上界不包含管理头大小，也不假定一个 slab 实际能容纳多少对象。
#define MAX_OBJECTS (2 * PGSIZE / REUSE_SIZE)

struct test_object {
    void *ptr;
    size_t size;
    unsigned int tag;
};

// 记录位于静态存储区，不消耗 SLUB 对象，也避免占用有限的内核栈。
static struct test_object objects[MAX_OBJECTS];

static uint8_t
test_pattern(unsigned int tag, size_t offset) {
    return (uint8_t)(tag * 37 + offset * 17 + (offset >> 8) * 29);
}

static void
test_disjoint(void) {
    for (size_t i = 0; i < MAX_OBJECTS; i ++) {
        if (objects[i].ptr == NULL) {
            continue;
        }
        uintptr_t a = (uintptr_t)objects[i].ptr;
        for (size_t j = i + 1; j < MAX_OBJECTS; j ++) {
            if (objects[j].ptr == NULL) {
                continue;
            }
            uintptr_t b = (uintptr_t)objects[j].ptr;
            // 使用地址差而非结束地址相加，检查区间不重叠且避免加法溢出。
            if (a <= b) {
                assert(b - a >= objects[i].size);
            } else {
                assert(a - b >= objects[j].size);
            }
        }
    }
}

static void
test_verify_all(void) {
    test_disjoint();
    for (size_t i = 0; i < MAX_OBJECTS; i ++) {
        if (objects[i].ptr == NULL) {
            continue;
        }
        const uint8_t *data = objects[i].ptr;
        for (size_t j = 0; j < objects[i].size; j ++) {
            uint8_t expected = test_pattern(objects[i].tag, j);
            if (data[j] != expected) {
                cprintf("[SLUB] data mismatch: object=%lu size=%lu offset=%lu\n",
                        (unsigned long)i, (unsigned long)objects[i].size,
                        (unsigned long)j);
            }
            assert(data[j] == expected);
        }
    }
}

static void
test_alloc_object(size_t index, size_t size, unsigned int tag) {
    assert(index < MAX_OBJECTS && objects[index].ptr == NULL);
    void *ptr = slub_alloc(size);
    if (ptr == NULL) {
        cprintf("[SLUB] allocation failed: object=%lu size=%lu\n",
                (unsigned long)index, (unsigned long)size);
    }
    assert(ptr != NULL);
    assert((uintptr_t)ptr % 16 == 0);
    objects[index].ptr = ptr;
    objects[index].size = size;
    objects[index].tag = tag;

    // 先检查不重叠，再写入请求长度内的每个字节，不读取初始数据。
    test_disjoint();
    uint8_t *data = ptr;
    for (size_t i = 0; i < size; i ++) {
        data[i] = test_pattern(tag, i);
    }
    test_verify_all();
}

static void
test_free_object(size_t index) {
    assert(index < MAX_OBJECTS && objects[index].ptr != NULL);
    test_verify_all();
    slub_free(objects[index].ptr);
    objects[index].ptr = NULL;
    // 每次释放后重查其他存活对象，定位释放操作造成的数据破坏。
    test_verify_all();
}

static void
test_zero_and_null(void) {
    size_t before = nr_free_pages();
    assert(slub_alloc(0) == NULL);
    assert(nr_free_pages() == before);
    slub_free(NULL);
    assert(nr_free_pages() == before);
    test_alloc_object(0, 1, 1);
    test_free_object(0);
    assert(nr_free_pages() == before);
    cprintf("[SLUB] 1 zero/null: PASS\n");
}

static void
test_size_boundaries(void) {
    static const size_t sizes[] = {
        1,
        15, 16, 17, 31, 32, 33, 63, 64, 65,
        127, 128, 129, 255, 256, 257, 511, 512, 513,
        1023, 1024, 1025, 2047, 2048, 2049,
        PGSIZE - 1, PGSIZE, PGSIZE + 1, 5000, 2 * PGSIZE + 1,
    };
    size_t before = nr_free_pages();
    for (size_t i = 0; i < ARRAY_SIZE(sizes); i ++) {
        test_alloc_object(0, sizes[i], (unsigned int)i + 1);
        test_free_object(0);
        // 每个独立请求释放后都应立即归还其占用的物理页。
        assert(nr_free_pages() == before);
    }
    cprintf("[SLUB] 2 size boundaries / large objects: PASS\n");
}

static void
test_data_isolation(void) {
    static const size_t sizes[] = {
        1, 16, 17, 64, 64, 64, 127, 256, 1025, 2048, 2049, 5000,
    };
    size_t before = nr_free_pages();
    for (size_t i = 0; i < ARRAY_SIZE(sizes); i ++) {
        test_alloc_object(i, sizes[i], (unsigned int)i + 1);
    }
    // 保留偶数项，交错释放和重新申请其他尺寸，反复检查存活数据。
    for (size_t i = 1; i < ARRAY_SIZE(sizes); i += 2) {
        test_free_object(i);
    }
    for (size_t i = 1; i < ARRAY_SIZE(sizes); i += 2) {
        test_alloc_object(i, i % 4 == 1 ? 33 : 5001, (unsigned int)i + 101);
    }
    for (size_t i = ARRAY_SIZE(sizes); i > 0; i --) {
        test_free_object(i - 1);
    }
    assert(nr_free_pages() == before);
    cprintf("[SLUB] 3 data isolation: PASS\n");
}

static void
test_cross_slab_reuse(void) {
    size_t before = nr_free_pages();
    for (size_t i = 0; i < MAX_OBJECTS; i ++) {
        test_alloc_object(i, REUSE_SIZE, (unsigned int)i + 1);
    }
    assert(before >= nr_free_pages());
    assert(before - nr_free_pages() > 1);

    size_t slab_count = 0, holes = 0;
    size_t occupied_pages = nr_free_pages();
    for (size_t i = 0; i < MAX_OBJECTS; i ++) {
        uintptr_t page = ROUNDDOWN((uintptr_t)objects[i].ptr, PGSIZE);
        bool keep = 1;
        for (size_t j = 0; j < i; j ++) {
            if (objects[j].ptr != NULL &&
                ROUNDDOWN((uintptr_t)objects[j].ptr, PGSIZE) == page) {
                keep = 0;
                break;
            }
        }
        if (keep) {
            slab_count ++;
        } else {
            // 按公开返回地址分组，每个单页 slab 保留一个存活对象。
            test_free_object(i);
            holes ++;
        }
    }
    assert(slab_count > 1 && holes > 0);
    // 尚有对象的 slab 不能被回收；此时已有 holes 个可复用槽位。
    assert(nr_free_pages() == occupied_pages);
    for (size_t i = 0; i < MAX_OBJECTS; i ++) {
        if (objects[i].ptr == NULL) {
            test_alloc_object(i, REUSE_SIZE, (unsigned int)i + 129);
            // 空位足够时，不接受先申请新页、最后再回收的行为。
            assert(nr_free_pages() == occupied_pages);
        }
    }
    for (size_t i = MAX_OBJECTS; i > 0; i --) {
        test_free_object(i - 1);
    }
    assert(nr_free_pages() == before);
    cprintf("[SLUB] 4 cross-slab reuse: PASS (%lu slabs, %lu reused slots)\n",
            (unsigned long)slab_count, (unsigned long)holes);
}

static void
test_page_reclaim(void) {
    static const size_t small_sizes[] = {16, 16, 64, 64, 256, 256, 1024, 1024, 2048};
    static const size_t large_sizes[] = {2049, 5000, 2 * PGSIZE + 1};
    size_t large_costs[ARRAY_SIZE(large_sizes)];
    size_t before = nr_free_pages(), large_total = 0;
    for (size_t i = 0; i < ARRAY_SIZE(small_sizes); i ++) {
        test_alloc_object(i, small_sizes[i], (unsigned int)i + 1);
    }
    for (size_t i = 0; i < ARRAY_SIZE(large_sizes); i ++) {
        size_t free_before = nr_free_pages();
        test_alloc_object(ARRAY_SIZE(small_sizes) + i, large_sizes[i],
                          (unsigned int)i + 101);
        assert(nr_free_pages() < free_before);
        large_costs[i] = free_before - nr_free_pages();
        large_total += large_costs[i];
    }

    // 部分释放后仍会验证所有存活对象；不假设小对象 slab 的容量。
    for (size_t i = 0; i < ARRAY_SIZE(small_sizes); i += 2) {
        test_free_object(i);
    }
    for (size_t i = 1; i < ARRAY_SIZE(small_sizes); i += 2) {
        test_free_object(i);
    }
    // 小对象全部释放后，只能保留大对象占用的页，不能缓存空 slab。
    assert(large_total <= before);
    assert(nr_free_pages() == before - large_total);
    for (size_t step = 0; step < ARRAY_SIZE(large_sizes); step ++) {
        size_t i = (step + 2) % ARRAY_SIZE(large_sizes);
        size_t free_before = nr_free_pages();
        test_free_object(ARRAY_SIZE(small_sizes) + i);
        // 页成本由实际分配前后差值取得，不依赖私有管理头大小。
        assert(nr_free_pages() == free_before + large_costs[i]);
    }
    assert(nr_free_pages() == before);
    cprintf("[SLUB] 5 physical page reclaim: PASS\n");
}

static void
test_failed_request(size_t size) {
    size_t before = nr_free_pages();
    void *ptr = slub_alloc(size);
    if (ptr != NULL) {
        cprintf("[SLUB] unexpected allocation success: size=%lu\n",
                (unsigned long)size);
    }
    assert(ptr == NULL);
    assert(nr_free_pages() == before);
    test_verify_all();
}

static void
test_failure_and_overflow(void) {
    size_t before = nr_free_pages();
    test_alloc_object(0, 37, 1);
    test_alloc_object(1, 5000, 2);

    size_t available = nr_free_pages();
    size_t max_size = (size_t)-1;
    // 此条件确保 +1 和乘法自身均不溢出，但请求比全部空闲页还多一页。
    assert(available < max_size / PGSIZE);
    size_t too_large = (available + 1) * PGSIZE;
    test_failed_request(too_large);

    // 同时覆盖头部加法与页对齐加法易溢出的长度，不假设头部尺寸。
    test_failed_request(max_size);
    test_failed_request(max_size - 1);
    test_failed_request(max_size - 15);
    test_failed_request(max_size - (PGSIZE - 1));

    // 失败后两个分配路径仍需可用，原有对象也应保持有效。
    test_alloc_object(2, 64, 3);
    test_alloc_object(3, 2 * PGSIZE + 1, 4);
    for (size_t i = 4; i > 0; i --) {
        test_free_object(i - 1);
    }
    assert(nr_free_pages() == before);
    cprintf("[SLUB] 6 allocation failure / overflow: PASS\n");
}

void
slub_check(void) {
    size_t before = nr_free_pages();
    for (size_t i = 0; i < MAX_OBJECTS; i ++) {
        assert(objects[i].ptr == NULL);
    }

    test_zero_and_null();
    test_size_boundaries();
    test_data_isolation();
    test_cross_slab_reuse();
    test_page_reclaim();
    test_failure_and_overflow();

    for (size_t i = 0; i < MAX_OBJECTS; i ++) {
        assert(objects[i].ptr == NULL);
    }
    assert(nr_free_pages() == before);
    cprintf("[SLUB] all tests passed!\n");
}
