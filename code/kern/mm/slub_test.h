#ifndef __KERN_MM_SLUB_TEST_H__
#define __KERN_MM_SLUB_TEST_H__

// lab2 2413497：独立 SLUB 测试；调用前应完成初始化且没有存活对象。
void slub_check(void);

#endif /* !__KERN_MM_SLUB_TEST_H__ */
