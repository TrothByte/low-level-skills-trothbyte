# Writing a WebAssembly Runtime from Scratch

Rules for building a correct WASM module loader/validator/interpreter.
Each entry: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE →
VERIFICATION → SOURCE. Registry ids: `wasm-core-spec` (WebAssembly Core Specification 2.0,
normative), `iso-c11-n1570` (C11, UB), `cwe` (MITRE CWE).

## 1. Module structure: sections, and their strict order

- **RULE**: A WASM binary is a flat sequence of sections in a fixed order: type, import,
  function, table, memory, global, export, start, element, code, data (custom sections may
  appear anywhere). Each section has an id byte, a varuint32 byte length, and a payload that
  must parse exactly to the declared length. Unknown section ids and out-of-order sections
  are a validation error, not a skip.
- **WHY AI GETS IT WRONG**: parses the binary as "find the section with this id" and
  tolerates duplicates or missing sections; ignores that the code section references
  function indices declared in the function section.
- **CORRECT REASONING**: the module is a single pass over sections in order; the code
  section must have exactly as many function bodies as the function section declares, and
  each body must parse fully (no trailing garbage). Malformed modules must be rejected at
  load/validation time, never half-executed.
- **EXAMPLE** (bad): a loader that reads the code section first and only later discovers
  there is no function section declaring the bodies, so it "guesses" the count.
- **COUNTEREXAMPLE** (good): a loader that walks sections in order, enforces one occurrence
  of each defined section, checks the declared length against actual bytes consumed, and
  cross-checks code-body count against the function section.
- **VERIFICATION**: feed truncated/mutated binaries; the loader must reject with an
  "invalid section"/"length mismatch" error and not crash.
- **SOURCE**: `wasm-core-spec` §4.1 (binary format), §3.2 (module structure, validation).

## 2. Validation happens before execution, per instruction

- **RULE**: every module must pass validation before any function runs. Validation is a
  type check over a type stack: each instruction's operand types are popped and its result
  types pushed, in a single linear pass per function body. Operand-stack underflow, `br`
  to an invalid label, and `call_indirect` with an unknown type index are validation errors.
- **WHY AI GETS IT WRONG**: treats type errors as "we will just trap at runtime" or skips
  validation entirely, so malformed bytecode is executed.
- **CORRECT REASONING**: validation is what guarantees the abstract machine cannot get
  stuck: a valid module never causes operand-stack underflow or type mismatch at runtime,
  so all remaining failure modes are well-defined traps (memory OOB, call_indirect index
  OOB, division by zero, unreachable). Interpreting without validating means one bad `i32.add`
  on an empty stack produces garbage instead of a defined failure.
- **EXAMPLE** (bad): executing `OP_I32_ADD` when the operand stack holds one value, adding
  it with fabricated "0".
- **COUNTEREXAMPLE** (good): a validation pass that simulates the type stack for the whole
  body and rejects the module before the interpreter runs a single instruction.
- **VERIFICATION**: validator rejects `(i32.add (i32.const 1))` (one operand missing) and
  `(call_indirect (type 5) (i32.const 0))` with type 5 out of range.
- **SOURCE**: `wasm-core-spec` §3.3 (validation), §4.5 (validation algorithm).

## 3. Linear memory: pages, bounds, and growth

- **RULE**: linear memory is an array of bytes sized in 64 KiB pages, initially zero-filled.
  Every `load`/`store` with effective address + access size outside `[0, memory.size)`
  traps with `out of bounds memory access`. `memory.grow` grows by a delta of pages, returns
  the old size in pages on success, or -1 on failure; new pages are zero-filled; memory
  never shrinks.
- **WHY AI GETS IT WRONG**: writes `mem[addr] = v` in C with no bounds check, or checks
  `addr < mem_size` but not `addr + size <= mem_size` (so a 4-byte read at `mem_size - 2`
  overruns). Treats memory.grow as "just call realloc and ignore failure".
- **CORRECT REASONING**: a WASM trap is a defined outcome, so the runtime MUST detect OOB
  and stop; an unchecked array index in C is UB (N1570 §6.5.6p8), which may read/write
  adjacent state or be optimized away. Check `addr > size || size - addr < n` before every
  access, compute growth with overflow and maximum-size checks, and zero new pages.
- **EXAMPLE** (bad): `int32_t v; memcpy(&v, mem + (size_t)addr, 4);` with `addr` from the
  guest and no bounds check.
- **COUNTEREXAMPLE** (good): `if (addr > m->size || m->size - addr < sizeof(v)) return TRAP_MEM_OOB;`
  before the copy; `memory.grow` rejects `delta > max_pages - current` and zeroes the new region.
- **VERIFICATION**: guest program loading/storing at `size - 1`, `size`, and `0xFFFFFFFF`
  must trap for the last two; grow-by-0 returns current size, grow beyond max returns -1.
- **SOURCE**: `wasm-core-spec` §3.3.4 (memory), §4.4.6 (memory instructions); `iso-c11-n1570`
  §6.5.6p8; `cwe` CWE-787, CWE-125.

## 4. Tables and call_indirect

- **RULE**: a table is an array of references. `call_indirect` pops the table index from
  the operand stack, traps on `index >= table.size` or a null entry, then checks the
  callee's type against the immediate type index (a runtime type check, because the table
  can be mutated); a mismatch traps with `indirect call type mismatch`.
- **WHY AI GETS IT WRONG**: indexes the table array with the untrusted guest value directly
  in C, or skips the runtime type check "because validation checked it".
- **CORRECT REASONING**: the type check on a `call_indirect` immediate is done at validation;
  the callee's actual type can only be checked at runtime. A bad index is a trap (defined),
  while `table[i]()` with `i >= length` in C is an OOB function-pointer read (UB, CWE-787)
  that usually jumps to garbage.
- **EXAMPLE** (bad): `i32 (*f)(i32) = table[(size_t)idx]; return f(arg);` with no length or
  null check.
- **COUNTEREXAMPLE** (good): pop index; `if (idx < 0 || (size_t)idx >= table_len) trap(CALL_INDIRECT_OOB);`
  then `if (!table[idx]) trap(CALL_INDIRECT_NULL);` then call.
- **VERIFICATION**: `(call_indirect (type 0) (i32.const 99))` on a 2-entry table traps; a
  null table slot traps; running the same binary under wasmtime must produce the same trap.
- **SOURCE**: `wasm-core-spec` §3.3.5 (tables), §4.4.4 (call_indirect); `cwe` CWE-787,
  CWE-476.

## 5. Stack machine semantics

- **RULE**: execution is a stack machine. Instructions pop operands and push results in a
  strictly typed order; the operand stack is per-frame and has a fixed maximum depth. A
  function's result is the value left on the stack at `return`.
- **WHY AI GETS IT WRONG**: models the stack as "some array we push/pop loosely" and pops
  without a depth check, so an underflow fabricates values and an overflow corrupts the
  frame above.
- **CORRECT REASONING**: with validation in place, runtime operand underflow is impossible
  for valid modules; the interpreter still needs a stack-overflow check (max depth) because
  a hostile module can legitimately reach it. Every `pop` on an interpreter stack with
  `sp == 0` is an OOB array access in C (UB), not a graceful failure.
- **EXAMPLE** (bad): `return stack[--sp];` with `sp` unsigned — empty stack wraps to a huge
  index and reads wild memory.
- **COUNTEREXAMPLE** (good): `pop` returns a trap on `sp == 0`; `push` traps on
  `sp >= STACK_CAP`; the interpreter checks the trap flag after every pop/push and stops.
- **VERIFICATION**: run a program that `add`s on an empty stack — the interpreter must
  trap or, under the real spec, the module must fail validation.
- **SOURCE**: `wasm-core-spec` §2.5 (stack machine), §3.3.3 (operand stack, max depth);
  `iso-c11-n1570` §6.5.6p8.

## 6. Traps are defined behavior; they are NOT C UB

- **RULE**: a trap (memory OOB, call_indirect OOB, null entry, divide-by-zero, unreachable,
  stack exhaustion) is a defined, atomic outcome: the current invocation and its callers
  are terminated with the trap, the instance is not corrupted, and other instances are
  unaffected. This is a trap, not undefined behavior — but an interpreter that implements
  traps with unchecked C array indexing turns each defined trap into C UB.
- **WHY AI GETS IT WRONG**: conflates "the guest traps" with "the host C program can do
  anything", and "fixes" traps by returning garbage, continuing execution, or crashing the
  host process.
- **CORRECT REASONING**: two layers: (1) the WASM abstract machine defines traps precisely;
  (2) the C implementation must realize each trap by a checked branch that produces a
  defined host-level outcome (error return / longjmp / exception), never by an out-of-bounds
  or null access. A trap leaves the host process alive and the instance reusable.
- **EXAMPLE** (bad): OOB guest store that overwrites the interpreter's own `table` pointer,
  corrupting later calls instead of trapping.
- **COUNTEREXAMPLE** (good): every failure path sets a `trap` code, the interpreter loop
  stops immediately, the host unwinds the call, and the instance state is left consistent.
- **VERIFICATION**: a guest that traps must leave exit code/diagnostic deterministic and
  identical to running the same module under wasmtime; no host crash, no silent success.
- **SOURCE**: `wasm-core-spec` §3.2.5 (traps), §2.2; `iso-c11-n1570` §6.5.6p8, Annex J.2;
  `cwe` CWE-119/CWE-787.

## 7. Interpreter design: decode → validate → execute

- **RULE**: the runtime has three separable stages: (1) binary decode (sections, LEB128
  numbers, code bodies), (2) validation (type checking, index bounds), (3) execution
  (stack machine + host calls). Stages must be run in order; a module is executed only if
  decode and validation succeed.
- **WHY AI GETS IT WRONG**: fuses decode and execution so malformed immediates are read
  during execution, or performs validation "lazily" per instruction, or stops on the first
  error and reports it as a runtime trap.
- **CORRECT REASONING**: a clean three-stage pipeline makes the failure modes disjoint:
  malformed module → load error; type-invalid module → validation error; valid module with
  bad dynamic operand → trap. It also makes bounds for every index (types, functions,
  globals, memories, tables) checkable in one pass.
- **EXAMPLE** (bad): an interpreter that, on an invalid immediate, "skips it and keeps
  running", so the same bytes are interpreted as an opcode on the next iteration.
- **COUNTEREXAMPLE** (good): decode produces a validated, typed IR/executable form; the
  executor consumes only that form and never re-parses raw bytes.
- **VERIFICATION**: fuzz by truncation and bit-flip: decode must never emit an execution
  error for a structurally bad input, and the executor must never parse bytes itself.
- **SOURCE**: `wasm-core-spec` §4.5 (validation before execution), §4.4 (execution).

## 8. Host functions: imports cross the trust boundary

- **RULE**: imported functions (e.g. `env.print`) are implemented by the host. Their
  signatures are fixed in the import section and must match the declared type exactly at
  call time. Arguments and results cross the guest/host boundary; host code must treat all
  guest-provided values as untrusted.
- **WHY AI GETS IT WRONG**: calls the C function pointer directly with raw guest stack
  values, ignoring the declared signature (e.g. passes a 64-bit value where a 32-bit
  pointer-like handle is expected), or trusts guest-provided pointers/indices inside host
  code.
- **CORRECT REASONING**: the host must marshal according to the declared type, and every
  guest-supplied index/pointer used by the host (e.g. into linear memory or the table) must
  be bounds-checked exactly like a guest load — a host that indexes guest memory without a
  check re-introduces C UB across the boundary. Signature mismatch must be rejected at
  validation, not silently reinterpreted.
- **EXAMPLE** (bad): a host `print` that does `puts(mem + guest_ptr)` with an unbounded
  guest pointer, letting a guest read the host's process memory past a non-null-terminated
  region.
- **COUNTEREXAMPLE** (good): the host validates the argument range against the current
  memory size and checks for a NUL byte within range before touching the string.
- **VERIFICATION**: a guest calling an imported function with the wrong argument type is
  rejected at validation; a guest passing an OOB pointer to a memory-reading import traps.
- **SOURCE**: `wasm-core-spec` §3.3.1 (imports), §2.5 (function types); `iso-c11-n1570`
  §7.24.2.4; `cwe` CWE-125, CWE-119.
