// GOOD: bounds-checked, trap-correct WASM-ish bytecode interpreter skeleton.
// Teaching map: TRAP_* = WASM trap, never silent garbage or C UB.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STACK_CAP 64u
#define PAGE_SIZE (64u * 1024u)
#define INIT_PAGES 1u
#define MAX_PAGES 32u
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

enum {
    TRAP_NONE = 0,
    TRAP_UNREACHABLE = 1,
    TRAP_MEM_OOB = 2,
    TRAP_CALL_INDIRECT_OOB = 3,
    TRAP_CALL_INDIRECT_NULL = 4,
    TRAP_STACK_UNDERFLOW = 5,
    TRAP_STACK_OVERFLOW = 6,
    TRAP_BAD_DECODE = 7
};

typedef struct vm {
    u8 *mem;
    size_t mem_size;
    i32 stack[STACK_CAP];
    size_t sp;
    i32 (*table[TABLE_LEN])(i32);
} vm;

static const char *trap_name(int t) {
    switch (t) {
    case TRAP_UNREACHABLE: return "unreachable";
    case TRAP_MEM_OOB: return "out-of-bounds memory";
    case TRAP_CALL_INDIRECT_OOB: return "call_indirect index oob";
    case TRAP_CALL_INDIRECT_NULL: return "call_indirect null entry";
    case TRAP_STACK_UNDERFLOW: return "stack underflow";
    case TRAP_STACK_OVERFLOW: return "stack overflow";
    case TRAP_BAD_DECODE: return "malformed bytecode";
    default: return "unknown";
    }
}

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

// Stack ops: push/pop trap instead of corrupting state.
static int vm_push(vm *v, i32 val, int *trap) {
    if (v->sp >= STACK_CAP) { *trap = TRAP_STACK_OVERFLOW; return 0; }
    v->stack[v->sp++] = val;
    return 1;
}

static int vm_pop(vm *v, i32 *out, int *trap) {
    if (v->sp == 0) { *trap = TRAP_STACK_UNDERFLOW; return 0; }
    *out = v->stack[--v->sp];
    return 1;
}

// Memory: address arithmetic checked before any access; endianness via memcpy.
static int vm_load_i32(vm *v, size_t addr, i32 *out, int *trap) {
    if (addr > v->mem_size || v->mem_size - addr < sizeof(i32)) {
        *trap = TRAP_MEM_OOB;
        return 0;
    }
    memcpy(out, v->mem + addr, sizeof(i32));
    return 1;
}

static int vm_store_i32(vm *v, size_t addr, i32 val, int *trap) {
    if (addr > v->mem_size || v->mem_size - addr < sizeof(i32)) {
        *trap = TRAP_MEM_OOB;
        return 0;
    }
    memcpy(v->mem + addr, &val, sizeof(i32));
    return 1;
}

// memory.grow: growth only, max-capped, overflow-checked, new pages zeroed.
static i32 vm_mem_grow(vm *v, u32 delta) {
    size_t old = v->mem_size / PAGE_SIZE;
    if (delta > MAX_PAGES - old) return -1;
    size_t new_size = (old + delta) * PAGE_SIZE;
    u8 *m = (u8 *)realloc(v->mem, new_size);
    if (!m) return -1;
    memset(m + v->mem_size, 0, new_size - v->mem_size);
    v->mem = m;
    v->mem_size = new_size;
    return (i32)old;
}

// call_indirect: index checked against table length, then entry checked non-null.
// type_index is a stub for the validation-phase type check of the callee.
static int vm_call_indirect(vm *v, size_t type_index, int *trap, i32 *result) {
    (void)type_index;
    i32 idx;
    if (!vm_pop(v, &idx, trap)) return 0;
    if (idx < 0 || (size_t)idx >= TABLE_LEN) { *trap = TRAP_CALL_INDIRECT_OOB; return 0; }
    i32 (*f)(i32) = v->table[(size_t)idx];
    if (!f) { *trap = TRAP_CALL_INDIRECT_NULL; return 0; }
    i32 arg;
    if (!vm_pop(v, &arg, trap)) return 0;
    *result = f(arg);
    return 1;
}

static u32 read_u32_at(const u8 *p) {
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

// decode -> execute loop; every failing decode/operand check becomes a trap.
static int vm_exec(vm *v, const u8 *code, size_t code_len, i32 *result) {
    size_t pc = 0;
    int trap = TRAP_NONE;
    v->sp = 0;
    while (pc < code_len && trap == TRAP_NONE) {
        u8 op = code[pc++];
        switch (op) {
        case OP_UNREACHABLE:
            trap = TRAP_UNREACHABLE;
            break;
        case OP_RET: {
            i32 r;
            if (!vm_pop(v, &r, &trap)) break;
            *result = r;
            return trap;
        }
        case OP_I32_CONST: {
            if (pc + 4 > code_len) { trap = TRAP_BAD_DECODE; break; }
            i32 val = (i32)read_u32_at(code + pc);
            pc += 4;
            if (!vm_push(v, val, &trap)) break;
            break;
        }
        case OP_I32_LOAD: {
            i32 a;
            if (!vm_pop(v, &a, &trap)) break;
            i32 val;
            if (!vm_load_i32(v, (size_t)a, &val, &trap)) break;
            if (!vm_push(v, val, &trap)) break;
            break;
        }
        case OP_I32_STORE: {
            i32 val;
            if (!vm_pop(v, &val, &trap)) break;
            i32 a;
            if (!vm_pop(v, &a, &trap)) break;
            if (!vm_store_i32(v, (size_t)a, val, &trap)) break;
            break;
        }
        case OP_MEM_GROW: {
            i32 delta;
            if (!vm_pop(v, &delta, &trap)) break;
            if (delta < 0) { trap = TRAP_BAD_DECODE; break; }
            i32 old = vm_mem_grow(v, (u32)delta);
            if (!vm_push(v, old, &trap)) break;
            break;
        }
        case OP_CALL_INDIRECT: {
            if (pc + 2 > code_len) { trap = TRAP_BAD_DECODE; break; }
            size_t type_index = (size_t)code[pc];
            pc += 2;
            i32 r;
            if (!vm_call_indirect(v, type_index, &trap, &r)) break;
            if (!vm_push(v, r, &trap)) break;
            break;
        }
        case OP_I32_ADD: {
            i32 b, a;
            if (!vm_pop(v, &b, &trap)) break;
            if (!vm_pop(v, &a, &trap)) break;
            if (!vm_push(v, a + b, &trap)) break;
            break;
        }
        case OP_DROP: {
            i32 d;
            if (!vm_pop(v, &d, &trap)) break;
            (void)d;
            break;
        }
        default:
            trap = TRAP_BAD_DECODE;
            break;
        }
    }
    if (trap == TRAP_NONE) {
        i32 r;
        if (!vm_pop(v, &r, &trap)) return trap;
        *result = r;
    }
    return trap;
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
    i32 result = 0;
    int trap = vm_exec(&v, p->code, p->len, &result);
    vm_free(&v);
    if (trap == TRAP_NONE) {
        printf("PASS %-16s result=%d\n", p->name, result);
        return 0;
    }
    printf("TRAP %-16s trap=%d (%s)\n", p->name, trap, trap_name(trap));
    return 100 + trap;
}
