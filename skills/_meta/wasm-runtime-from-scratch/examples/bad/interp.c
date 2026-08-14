// BAD: same WASM-ish interpreter skeleton with the checks removed.
// Teaching map: every unchecked index / missing trap is C UB -> arbitrary behavior.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STACK_CAP 64u
#define PAGE_SIZE (64u * 1024u)
#define INIT_PAGES 1u
#define TABLE_LEN 2u

typedef uint8_t u8;
typedef uint32_t u32;
typedef int32_t i32;

enum {
    OP_UNREACHABLE = 0x00,
    OP_RET = 0x0f,
    OP_CALL_INDIRECT = 0x11,
    OP_I32_LOAD = 0x28,
    OP_I32_STORE = 0x36,
    OP_MEM_GROW = 0x40,
    OP_I32_CONST = 0x41,
    OP_I32_ADD = 0x6a,
    OP_DROP = 0x1a
};

typedef struct vm {
    u8 *mem;
    size_t mem_size;
    i32 stack[STACK_CAP];
    int32_t sp;
    i32 (*table[TABLE_LEN])(i32);
} vm;

static i32 fn_inc(i32 x) { return x + 1; }
static i32 fn_sq(i32 x) { return x * x; }

static int vm_init(vm *v) {
    v->mem = (u8 *)calloc(INIT_PAGES * PAGE_SIZE, 1);
    if (!v->mem) return -1;
    v->mem_size = (size_t)INIT_PAGES * PAGE_SIZE;
    v->sp = 0;
    v->table[0] = fn_inc;
    v->table[1] = fn_sq;
    return 0;
}

static void vm_free(vm *v) {
    free(v->mem);
    v->mem = NULL;
    v->mem_size = 0;
}

// No overflow check: writes past the operand stack.
static void push(vm *v, i32 val) { v->stack[v->sp++] = val; }

// No underflow check: empty stack indexes before the array -> reads garbage.
static i32 pop(vm *v) { return v->stack[--v->sp]; }

// No bounds check: OOB read/write on linear memory.
static i32 load_i32(vm *v, i32 a) {
    i32 val;
    memcpy(&val, v->mem + (size_t)a, sizeof(i32));
    return val;
}

static void store_i32(vm *v, i32 a, i32 val) {
    memcpy(v->mem + (size_t)a, &val, sizeof(i32));
}

// No max check, no overflow check, no zeroing of new pages.
static i32 mem_grow(vm *v, u32 delta) {
    i32 old = (i32)(v->mem_size / PAGE_SIZE);
    v->mem_size = (size_t)(old + delta) * PAGE_SIZE;
    u8 *m = (u8 *)realloc(v->mem, v->mem_size);
    v->mem = m;
    return old;
}

// No index check on the table, no null check, no type check.
static i32 call_indirect(vm *v) {
    i32 idx = pop(v);
    i32 arg = pop(v);
    i32 (*f)(i32) = v->table[(size_t)idx];
    return f(arg);
}

static u32 read_u32_at(const u8 *p) {
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

// No trap state: the loop keeps executing no matter what went wrong.
static i32 vm_exec(vm *v, const u8 *code, size_t code_len) {
    size_t pc = 0;
    v->sp = 0;
    while (pc < code_len) {
        u8 op = code[pc++];
        switch (op) {
        case OP_UNREACHABLE:
            return 0;
        case OP_RET:
            return pop(v);
        case OP_I32_CONST: {
            i32 val = (i32)read_u32_at(code + pc);
            pc += 4;
            push(v, val);
            break;
        }
        case OP_I32_LOAD:
            push(v, load_i32(v, pop(v)));
            break;
        case OP_I32_STORE: {
            i32 val = pop(v);
            i32 a = pop(v);
            store_i32(v, a, val);
            break;
        }
        case OP_MEM_GROW: {
            i32 delta = pop(v);
            push(v, mem_grow(v, (u32)delta));
            break;
        }
        case OP_CALL_INDIRECT: {
            pc += 2;
            push(v, call_indirect(v));
            break;
        }
        case OP_I32_ADD: {
            i32 b = pop(v);
            i32 a = pop(v);
            push(v, a + b);
            break;
        }
        case OP_DROP:
            pop(v);
            break;
        default:
            return 0;
        }
    }
    return pop(v);
}

typedef struct test_prog { const char *name; const u8 *code; size_t len; } test_prog;

static const u8 t1[] = {
    OP_I32_CONST, 8, 0, 0, 0, OP_I32_CONST, 4, 0, 0, 0, OP_I32_STORE,
    OP_I32_CONST, 8, 0, 0, 0, OP_I32_LOAD,
    OP_I32_CONST, 12, 0, 0, 0, OP_I32_ADD,
    OP_RET
};
static const u8 t2[] = {
    OP_I32_CONST, 5, 0, 0, 0, OP_I32_CONST, 1, 0, 0, 0,
    OP_CALL_INDIRECT, 0, 0,
    OP_RET
};
static const u8 t3[] = { OP_I32_CONST, 0, 0, 1, 0, OP_I32_LOAD, OP_RET };
static const u8 t4[] = {
    OP_I32_CONST, 0, 0, 0, 0, OP_I32_CONST, 9, 0, 0, 0,
    OP_CALL_INDIRECT, 0, 0,
    OP_RET
};
static const u8 t5[] = { OP_I32_ADD, OP_RET };
static const u8 t6[] = { OP_I32_CONST, 100, 0, 0, 0, OP_MEM_GROW, OP_RET };

static const test_prog tests[] = {
    {"mem-roundtrip", t1, sizeof t1},
    {"call-indirect", t2, sizeof t2},
    {"load-oob", t3, sizeof t3},
    {"callind-oob", t4, sizeof t4},
    {"stack-underflow", t5, sizeof t5},
    {"grow-over-max", t6, sizeof t6},
};
#define NTESTS (sizeof tests / sizeof tests[0])

int main(int argc, char **argv) {
    if (argc != 2) { fprintf(stderr, "usage: interp <1..%d>\n", (int)NTESTS); return 64; }
    int t = atoi(argv[1]);
    if (t < 1 || (size_t)t > NTESTS) { fprintf(stderr, "no such test\n"); return 64; }
    vm v;
    if (vm_init(&v) != 0) { fprintf(stderr, "vm init failed\n"); return 70; }
    const test_prog *p = &tests[t - 1];
    i32 result = vm_exec(&v, p->code, p->len);
    vm_free(&v);
    printf("RESULT %-16s value=%d\n", p->name, result);
    return (int)((u32)result & 0xffu);
}
