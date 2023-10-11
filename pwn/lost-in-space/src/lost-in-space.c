#include <errno.h>
#include <fcntl.h>
#include <linux/audit.h>
#include <linux/bpf.h>
#include <linux/filter.h>
#include <linux/prctl.h>
#include <linux/seccomp.h>
#include <stddef.h>
#include <sys/mman.h>
#include <sys/time.h>

#include "syscall.h"

#define SIZE(x) (sizeof(x) / sizeof(*x))

#define PAGE_BITS 12
#define PAGE_SIZE 0x1000
#define PAGE_ALIGN(x) ((x) & ~(PAGE_SIZE - 1))
#define PAGE_ALIGN_UP(x) PAGE_ALIGN((x) + (PAGE_SIZE - 1))

#define VIRT_MAX 0x00007fffffffffff

#define GRAPH_NODES 1000
#define GRAPH_EDGES 2500
#define MAX_PATH_LEN 200

typedef unsigned char u8;
typedef unsigned u32;
typedef unsigned long u64;
typedef int i32;

struct node {
    u8 dest;
    i32 n_links;
    struct node *links[];
};

extern u64 prog_start;
extern u64 prog_end;

struct node *nodes[GRAPH_NODES];

i32 main(void);
void __attribute__((section(".touser"), __used__, __noreturn__))
touser(char *code);
void randomize(void);
i32 add_link(struct node *n1, struct node *n2);
void randomize_links(struct node *n1);
void sandbox(u64 safe_page);
u64 randint(u64 lo, u64 hi);
void printhex(u64 x);

__attribute__((naked, noreturn)) void entry(void) {
    asm volatile("subq $8, %%rsp\n"
                 "call main\n"
                 "movl %%eax, %%edi\n"
                 "movl %0, %%eax\n"
                 "syscall\n" ::"g"(SYS_exit));
}

i32 rand_fd;

i32 main(void) {
    struct itimerval timer = {
        .it_interval = {0, 0},
        .it_value = {5, 0},
    };
    syscall(SYS_setitimer, ITIMER_REAL, &timer, 0);
    rand_fd = syscall(SYS_open, "/dev/urandom", O_RDONLY);
    randomize();

    char *code = ((char *)nodes[0]) + PAGE_SIZE / 2;
    syscall(SYS_read, 0, code, PAGE_SIZE / 2);
    sandbox((u64)nodes[MAX_PATH_LEN]);
    touser(code);
}

void __attribute__((section(".touser"), __used__, __noreturn__))
touser(char *code) {
    register u64 rsp asm("rsp");
    syscall(SYS_munmap, (u64)&prog_start,
            PAGE_ALIGN_UP((u64)&prog_end) - (u64)&prog_start);
    syscall(SYS_munmap, PAGE_ALIGN(rsp) - 0x20000, 0x21000);

    asm volatile("xor %%eax, %%eax\n"
                 "xor %%ecx, %%ecx\n"
                 "xor %%ebx, %%ebx\n"
                 "xor %%esp, %%esp\n"
                 "xor %%ebp, %%ebp\n"
                 "xor %%esi, %%esi\n"
                 "xor %%edi, %%edi\n"
                 "xor %%r8d, %%r8d\n"
                 "xor %%r9d, %%r9d\n"
                 "xor %%r10d, %%r10d\n"
                 "xor %%r11d, %%r11d\n"
                 "xor %%r12d, %%r12d\n"
                 "xor %%r13d, %%r13d\n"
                 "xor %%r14d, %%r14d\n"
                 "xor %%r15d, %%r15d\n"
                 "jmpq *%%rdx\n" ::"rdx"(code));
    __builtin_unreachable();
}

void randomize(void) {
    for (i32 i = 0; i < GRAPH_NODES; i++) {
        u64 addr = randint(prog_end >> PAGE_BITS, VIRT_MAX >> PAGE_BITS)
                   << PAGE_BITS;
        syscall(SYS_mmap, addr, PAGE_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC,
                MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
        nodes[i] = (struct node *)addr;
    }

    nodes[MAX_PATH_LEN]->dest = 1;
    /* MST */
    for (i32 i = 0; i < GRAPH_NODES - 1; i++)
        add_link(nodes[i], nodes[i + 1]);

    i32 total = 0;
    while (total < GRAPH_EDGES)
        total += add_link(nodes[randint(0, GRAPH_NODES - 1)],
                          nodes[randint(0, GRAPH_NODES - 1)]);

    for (i32 i = 0; i < GRAPH_NODES; i++)
        randomize_links(nodes[i]);

    for (i32 i = 1; i < GRAPH_NODES; i++) {
        if (i != MAX_PATH_LEN)
            syscall(SYS_mprotect, nodes[i], PAGE_SIZE, PROT_READ);
    }
}

i32 add_link(struct node *n1, struct node *n2) {
    if (n1 == n2)
        return 0;
    for (i32 i = 0; i < n1->n_links; i++) {
        if (n1->links[i] == n2)
            return 0;
    }
    n1->links[n1->n_links++] = n2;
    n2->links[n2->n_links++] = n1;
    return 1;
}

void randomize_links(struct node *n1) {
    for (i32 i = 0; i < n1->n_links-1; i++) {
        i32 j = randint(i, n1->n_links-1);
        struct node *tmp = n1->links[i];
        n1->links[i] = n1->links[j];
        n1->links[j] = tmp;
    }
}

void sandbox(u64 safe_page) {
    struct sock_filter filter[] = {
        BPF_STMT(BPF_LD | BPF_W | BPF_ABS,
                 (offsetof(struct seccomp_data, instruction_pointer))),
        BPF_JUMP(BPF_JMP | BPF_JGE | BPF_K, (u64)safe_page, 0, 2),
        BPF_JUMP(BPF_JMP | BPF_JGT | BPF_K, (u64)safe_page + PAGE_SIZE, 1, 0),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),

        BPF_STMT(BPF_LD | BPF_W | BPF_ABS,
                 (offsetof(struct seccomp_data, arch))),
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, AUDIT_ARCH_X86_64, 0, 3),
        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, (offsetof(struct seccomp_data, nr))),
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, SYS_munmap, 0, 1),

        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),

        BPF_STMT(BPF_RET | BPF_K,
                 SECCOMP_RET_ERRNO | (EPERM & SECCOMP_RET_DATA)),
    };
    struct sock_fprog prog = {
        .len = sizeof filter / sizeof *filter,
        .filter = filter,
    };

    syscall(SYS_prctl, PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
    syscall(SYS_seccomp, SECCOMP_SET_MODE_FILTER, 0, &prog);
}

u64 randbuf[0x1000];
u32 randpos = SIZE(randbuf);
u64 randint(u64 lo, u64 hi) {
    u64 diff = hi - lo;
    u64 mask = ~0UL >> __builtin_clzl(diff);
    u64 n;
    do {
        if (randpos >= SIZE(randbuf)) {
            randpos = 0;
            syscall(SYS_read, rand_fd, randbuf, sizeof randbuf);
        }
        n = lo + (randbuf[randpos++] & mask);
    } while (n > hi);
    return n;
}

void printhex(u64 x) {
    char buf[17];
    i32 i = sizeof buf - 1;
    buf[i--] = '\n';
    __builtin_memset(buf, ' ', i + 1);
    do {
        i32 d = x & 0xf;
        buf[i--] = d > 9 ? 'a' + (d - 10) : '0' + d;
        x >>= 4;
    } while (x > 0);
    syscall(SYS_write, 1, buf, sizeof buf);
}
