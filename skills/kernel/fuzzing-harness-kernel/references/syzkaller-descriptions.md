# Kernel Fuzzing: Syzkaller Descriptions and Setup Paths

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE →
COUNTEREXAMPLE → VERIFICATION → SOURCE. Uncertainty marked KNOWN / INFERRED /
UNVERIFIED.

## 1. A syscall description without setup calls fuzzes error paths only

- **RULE**: syzkaller generates syscall programs from syzlang descriptions;
  a description that names the interesting ioctls but not the opens/binds/
  mounts that precede them yields programs that fail in setup and never
  reach the target logic. KNOWN (syzkaller docs: descriptions, resources).
- **WHY AI GETS IT WRONG**: agents list the target syscalls and stop;
  coverage then saturates in the error-return code paths.
- **CORRECT REASONING**: model the state, not just the calls: declare the
  resources (fd of the opened device, socket fd, mount fd) and the setup
  calls that produce them, then the fuzzable calls consume them.
- **EXAMPLE** (bad): a `net` description with `syz_emit_packet` calls but no
  `socket`/`bind` resource production.
- **COUNTEREXAMPLE** (good): `examples/good/syzlang_descs.txt` chains
  open -> ioctl-setup -> fuzzable op via resource types.
- **VERIFICATION**: `syz-sysgen` compiles the descriptions; coverage growth
  on the target functions is observed in the dashboard (documented).
- **SOURCE**: syzkaller-docs (descriptions, syscall descriptions for Linux)
  [proposed].

## 2. Resources are types with lifecycles, not magic values

- **RULE**: syzlang resource types (`resource fd_netdev[fd]`) teach
  syzkaller which syscall returns an object usable by another; the fuzzer
  then builds valid chains. A description using raw integers for fds
  produces invalid programs. KNOWN (syzkaller docs, resource model).
- **WHY AI GETS IT WRONG**: fd/object relationships are flattened to
  `int` arguments; the fuzzer can never construct a valid handle.
- **CORRECT REASONING**: define resources for every object the target
  consumes, mark the producing and consuming syscalls, and let syzkaller
  synthesize the chains.
- **EXAMPLE** (bad): a description where the ioctl's fd argument is a plain
  `int` with no producer.
- **COUNTEREXAMPLE** (good): `resource fd_vnet[fd]` produced by the open
  syscall and consumed by the ioctl; programs contain the open first.
- **VERIFICATION**: compare generated programs (with and without the
  resource chain) for valid open->ioctl order.
- **SOURCE**: syzkaller-docs (syzlang syntax, resources) [proposed].

## 3. Descriptions must compile before the fuzzer can run

- **RULE**: syzlang is checked by `syz-sysgen` at build; an invalid
  description aborts generation. Writing "nice-looking" descriptions that
  never passed sysgen is not a harness. KNOWN (syzkaller build process).
- **WHY AI GETS IT WRONG**: agents treat the description as documentation;
  the compile step is skipped.
- **CORRECT REASONING**: run the generator, fix its errors, and keep the
  generated `.go` files deterministic across the kernel version.
- **EXAMPLE** (bad): a description with an undefined flag enum or a syntax
  typo, never run through sysgen.
- **COUNTEREXAMPLE** (good): `syz-sysgen` clean; the generated syscall
  table includes the new calls.
- **VERIFICATION**: the sysgen command is in the evals "target" section
  (RESEARCHED).
- **SOURCE**: syzkaller-docs (setup, internals) [proposed].
