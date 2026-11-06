
#include <trap.h>

extern int printf(const char *, ...);
#define cprintf printf
#define CAUSE_ILLEGAL_INSTRUCTION 2
#define CAUSE_BREAKPOINT 3

static int fallback_count;
void print_trapframe(struct trapframe *tf) {
    fallback_count++;
    printf("Fallback: cause=%llu\n", tf->cause);
}
void exception_handler(struct trapframe *tf) {
    switch (tf->cause) {
    case CAUSE_ILLEGAL_INSTRUCTION:
    case CAUSE_BREAKPOINT: {
        if (tf->cause == CAUSE_ILLEGAL_INSTRUCTION) {
            cprintf("Illegal instruction at %p\n", (void *)tf->epc);
        } else {
            cprintf("Breakpoint at %p\n", (void *)tf->epc);
        }

        /*
         * Lab scope: readable instruction memory, 16/32-bit instructions.
         * Read a halfword because a compressed instruction is only 2 bytes
         * and a 32-bit instruction may start at a 2-byte-aligned address.
         */
        uint16_t insn = *(const volatile uint16_t *)tf->epc;
        tf->epc += ((insn & 0x3) == 0x3) ? 4 : 2;
        break;
    }
    default:
        print_trapframe(tf);
        break;
    }
}

static int check(uintptr_t cause, uint16_t *instruction,
                 uintptr_t advance, int fallback) {
    struct trapframe tf = {0};
    tf.cause = cause;
    tf.epc = (uintptr_t)instruction;
    tf.status = 0x1234;
    tf.gpr.a0 = 0x5678;

    uintptr_t before = tf.epc;
    int old_count = fallback_count;
    exception_handler(&tf);

    int ok = tf.epc == before + advance
          && fallback_count == old_count + fallback
          && tf.cause == cause
          && tf.status == 0x1234
          && tf.gpr.a0 == 0x5678;

    printf("cause=%llu, advance=%llu: %s\n",
           cause, advance, ok ? "PASS" : "FAIL");
    return !ok;
}

int main(void) {
    uint16_t compressed_break[] = {0x9002};
    uint16_t normal_break[] = {0x0073, 0x0010};
    uint16_t illegal32[] = {0x007b, 0x0000};
    uint16_t illegal16[] = {0x0000};
    int failures = 0;

    failures += check(3, compressed_break, 2, 0);
    failures += check(3, normal_break, 4, 0);
    failures += check(2, illegal32, 4, 0);
    failures += check(2, illegal16, 2, 0);
    failures += check(20, (uint16_t *)0, 0, 1);

    printf("Result: %d/5 passed\n", 5 - failures);
    return failures != 0;
}
