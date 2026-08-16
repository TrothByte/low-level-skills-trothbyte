# Reading Opaque Verifier Logs — Rules

Format per rule: RULE / WHY AI GETS IT WRONG / CORRECT REASONING / EXAMPLE /
COUNTEREXAMPLE / VERIFICATION / SOURCE.

## 1. The log tail is a summary; the failure is just above it

- **RULE**: A verifier dump ends with `processed N insns (limit 1000000)
  max_states_per_insn ...` — a summary. The actual rejection message and the
  failing instruction appear a few lines earlier, and the register state is
  printed immediately above the message.
- **WHY AI GETS IT WRONG**: models read the end of a dump first; the
  `processed N insns` tail looks like "the error", so the real message is
  skipped. Starovoitov describes verifier output as "huge dumps of data that
  obscure the actual source of the problem".
- **CORRECT REASONING**: parse the dump from the tail backwards: find the
  line that is a message (contains `invalid access`, `!read_ok`, `unbounded
  memory access`, `BPF program is too large`, `misaligned access`, ...); walk
  back one line to the register state; walk back to the last instruction line
  (`N: (...) mnemonic`) — that instruction is the failure site.
- **EXAMPLE** (bad): a program rejected with
  `R2 invalid access to packet` — the agent reads the `processed N insns`
  tail and reports "program too complex" instead of fixing the packet access.
- **COUNTEREXAMPLE** (good): the agent reads the message line, the register
  line (`R2(id=0,off=0,r=0)`), and the instruction line
  (`2: (79) r0 = *(u64 *)(r2 + 0)`), and fixes the missing bounds check.
- **VERIFICATION**: extract the failing instruction index from the log tail
  and the register state above the message
  (`examples/good/good_verifier_log_iteration.py`, step 1).
- **SOURCE**: lwn-1075067 (verifier errors "are insane" — huge dumps that
  obscure the cause); ebpf-docs: kernel/bpf/verifier.c (log printing and
  `processed %d insns` tail).

## 2. The message names a register; the register names the fix

- **RULE**: Verifier messages are of the form `R<n> <problem>` (e.g. `R0
  invalid mem access`, `R2 invalid access to packet`, `R5 !read_ok`). The
  named register's printed state is the reason.
- **WHY AI GETS IT WRONG**: the message text suggests a generic cause ("packet
  access") and the model proposes a generic change anywhere in the program,
  instead of tracing the named register.
- **CORRECT REASONING**: read the register state line: `PTR_TO_PACKET(id=0,
  off=0,r=0)` — the `r` field is the proven safe range. `r=0` means no bounds
  were established before the access; the fix must establish them (a
  `data_end` comparison) on the path the verifier sees.
- **EXAMPLE** (bad): message `R2 invalid access to packet, off=0 size=8,
  R2(id=0,off=0,r=0)` — the agent "fixes" by removing an unrelated helper call.
- **COUNTEREXAMPLE** (good): the agent reads `r=0`, locates the instruction
  that created `R2` as `PTR_TO_PACKET`, and inserts the bounds check before
  the access.
- **VERIFICATION**: bisect the register state to its creating instruction
  (iteration step 3 in the simulation).
- **SOURCE**: lwn-1075067 (Starovoitov: messages like "register is not init"
  leave programmers with no idea what to do — but the register state that
  follows does); ebpf-docs: Documentation/bpf/verifier.rst.

## 3. `register is not init` style messages are underspecified on purpose

- **RULE**: Some verifier messages are deliberately terse
  (`R5 !read_ok`, `register is not init`) because the verifier is a security
  boundary, not a tutor. The register state line above the message is the
  actual diagnostic.
- **WHY AI GETS IT WRONG**: an underspecified message triggers an
  underspecified "fix" (rewrite, delete code, or give up).
- **CORRECT REASONING**: map the message to the register it names and to the
  rule it violates (uninitialized read, invalid access, unbounded offset).
  `!read_ok` means a register was read before any write on some path —
  initialize it before use on every path.
- **EXAMPLE** (bad): giving up on `R5 !read_ok` because "the log is insane".
- **COUNTEREXAMPLE** (good): initializing the local before the helper call
  that reads it and re-loading with `bpftool prog load -d`.
- **VERIFICATION**: `bpftool prog load -d` on target prints the state line;
  fix to the named register's initialization.
- **SOURCE**: lwn-1075067 ("They say things like 'register is not init' —
  what is a programmer supposed to do with that?"); ebpf-docs:
  kernel/bpf/verifier.c.

## 4. Never read only the last line

- **RULE**: The last line of a verifier dump is never the actionable error;
  the actionable error is the message line plus the register state plus the
  failing instruction, all in the tail but above the summary.
- **WHY AI GETS IT WRONG**: tail-biased reading plus opaque formatting
  produces "the program is too large/complex" — the documented give-up.
- **CORRECT REASONING**: structured extraction — message line, register line,
  instruction line, instruction index — turns the dump into a bug report.
- **EXAMPLE** (bad): `BPF program is too large. Processed 1000000 insn`
  misread as the real error when the real error is a missing null check two
  lines above.
- **COUNTEREXAMPLE** (good): the agent reports "insn 2: R2 invalid access to
  packet" and fixes it.
- **VERIFICATION**: the extraction step in the simulation returns the correct
  failing instruction index.
- **SOURCE**: lwn-1075067; ebpf-docs.
