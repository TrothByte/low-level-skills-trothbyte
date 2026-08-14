# x86-64 Registers and Addressing — Reference Rules

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE
→ VERIFICATION → SOURCE. Source ids refer to registry/sources.yaml.

## 1. Register set and rflags bit positions

- **RULE**: 64-bit long mode has 16 GPRs: `rax rbx rcx rdx rsi rdi rbp rsp r8-r15`,
  plus `rip` (instruction pointer) and `rflags`. Flag bits are fixed: CF=0, PF=2,
  AF=4, ZF=6, SF=7, TF=8, IF=9, DF=10, OF=11, plus system flags (IOPL, NT, RF, VM...).
  Each GPR exposes 8/16/32-bit views: `al/ax/eax`, `r8b/r8w/r8d`, etc.
- **WHY AI GETS IT WRONG**: invents flag bit numbers (e.g. "OF is bit 0"); confuses
  IF (interrupt flag) with OF; treats `rip` as a readable GPR; forgets `r8-r15`
  exist and require REX.
- **CORRECT REASONING**: the flags form a bitmask — `pushfq`/`popq` dumps it, and
  CF is bit 0, ZF bit 6, SF bit 7, OF bit 11. `rip` is only usable as the base of
  RIP-relative addressing (or read via `call`/`lea`).
- **EXAMPLE** (bad):
  ```asm
  pushfq
  popq    %rax
  testq   $1, %rax        # agent claims this tests OF; it tests CF
  ```
- **COUNTEREXAMPLE** (good):
  ```asm
  cmpq    %rsi, %rdi
  jo      .overflow        # OF branch — no bit fiddling needed
  ```
- **VERIFICATION**: `gcc -c` + `objdump -d`; gdb `p/x $rflags` shows 0x202 (IF set)
  as the common idle value — bits 9/1 fixed.
- **SOURCE**: intel-sdm Vol.1 §3.4, §3.4.3; amd64-apm Vol.1 §3.1.

## 2. 32-bit writes zero-extend; 8/16-bit writes preserve the upper bits

- **RULE**: in 64-bit mode, any instruction that writes a 32-bit GPR zero-extends
  the upper 32 bits of the 64-bit register. An 8- or 16-bit write preserves the
  untouched upper bits.
- **WHY AI GETS IT WRONG**: assumes 32-bit mode semantics ("upper bits stay
  garbage") or assumes `movw` zero-extends like `movl`.
- **CORRECT REASONING**: `movl $1, %eax` → `rax = 1`. `movw $1, %ax` → only bits
  15:0 change; bits 16:63 keep their old value. This is AMD64-specific — 32-bit x86
  and AArch64 behave differently.
- **EXAMPLE** (bad):
  ```asm
  movq    $0x1122334455667788, %rax
  movw    $0, %ax              # bits 16:63 still hold 0x112233445566
  ```
- **COUNTEREXAMPLE** (good):
  ```asm
  xorl    %eax, %eax           # clears all 64 bits (zero-extension)
  movl    $0, %eax             # same effect, explicit
  ```
- **VERIFICATION**: `gcc -c` then gdb `p/x $rax` after each step; or objdump shows
  the same `B8/31 C0` encodings used by compilers for full-width clears.
- **SOURCE**: intel-sdm Vol.1 §3.4.1.1, §3.4.1.2; amd64-apm Vol.1 §3.1.2.

## 3. Operand-size suffixes and the default operand size

- **RULE**: GAS AT&T suffixes: `b`=8-bit, `w`=16-bit, `l`=32-bit, `q`=64-bit. The
  default operand size in 64-bit mode is 32 bits; 64-bit requires REX.W, 16-bit
  requires a 66h prefix.
- **WHY AI GETS IT WRONG**: reads `l` as "long, therefore 64-bit" because C `long`
  is 8 bytes on Linux x86-64; or reads `b` as "byte=32-bit".
- **CORRECT REASONING**: the suffix must match the register width. `movl (%rdi),%ax`
  is an error ("incorrect register used with `l' suffix"); `movl (%rdi),%eax` is
  32-bit and zero-extends; `movq (%rdi),%rax` is 64-bit.
- **EXAMPLE** (bad):
  ```asm
  movl    (%rdi), %ax     # as: Error: incorrect register `%ax' used with `l' suffix
  ```
- **COUNTEREXAMPLE** (good):
  ```asm
  movl    (%rdi), %eax    # 32-bit load, zero-extended to %rax
  movq    (%rdi), %rax    # full 64-bit load
  ```
- **VERIFICATION**: `gcc -c` exit code for the mismatch; objdump shows `B8 01 00 00
  00` (`movl $1,%eax`), `66 B8 01 00` (`movw`), `48 C7 C0 ...` (`movq`, REX.W).
- **SOURCE**: intel-sdm Vol.2 (66h prefix, REX.W); binutils-docs GNU `as`; agner-fog.

## 4. Addressing-mode forms

- **RULE**: memory operands have the forms: absolute disp, base+disp, base+index*scale
  +disp, index*scale+disp, and RIP-relative. Scale is 1, 2, 4, or 8. AT&T writes
  `disp(base,index,scale)`; Intel writes `[base+index*scale+disp]`.
- **WHY AI GETS IT WRONG**: mixes AT&T and Intel operand order; uses scale 3 or 5;
  tries two base registers; uses `rsp` as an index.
- **CORRECT REASONING**: one base + one index max; index*3 is not encodable (use
  index*2+base or multiply); the SIB index field 100b means "no index", so `rsp`
  cannot be an index — `as` rejects it, while `r12` is encodable via REX.X.
- **EXAMPLE** (bad):
  ```asm
  movq    (%rax, %rbx, 3), %rcx    # as: expecting scale factor of 1, 2, 4, or 8: got `3'
  movq    (%rax, %rsp, 2), %rcx    # as: not a valid base/index expression
  ```
- **COUNTEREXAMPLE** (good):
  ```asm
  movq    (%rax, %rbx, 4), %rcx    # index*4
  movq    (%rax, %rbx, 2), %rcx    # index*2; *3 must be expressed differently
  ```
- **VERIFICATION**: `gcc -c` exit codes and error text; objdump of the valid forms.
- **SOURCE**: intel-sdm Vol.1 §3.7.5; amd64-apm Vol.1 §3.4.

## 5. RIP-relative addressing and disp32 sign-extension

- **RULE**: RIP-relative is the default for position-independent code: `sym(%rip)`
  encodes a disp32 measured from the END of the instruction (the next RIP). It is a
  memory operand with no base/index. Any 32-bit displacement in base/index addressing
  is sign-extended to 64 bits.
- **WHY AI GETS IT WRONG**: measures the displacement from the start of the
  instruction; assumes absolute symbol addresses are safe in PIE; computes the disp32
  by hand and gets every instruction's length wrong.
- **CORRECT REASONING**: `lea foo(%rip),%rax` yields the address of `foo` at any load
  address; the relocation (`R_X86_64_PC32` / `IMAGE_REL_AMD64_REL32`) is resolved by
  the linker. Disp32 sign-extension is why `-2(%rdi)` and `0xFFFFFFFF(%rdi)` both
  mean "16 bytes before" the same base, not "4 GiB away".
- **EXAMPLE** (bad): computing a displacement as `target - instruction_start`
  without adding the instruction length.
- **COUNTEREXAMPLE** (good):
  ```asm
  leaq    my_data(%rip), %rax    # linker resolves the disp32
  movq    -8(%rdi), %rbx         # disp32 -8 sign-extends to -8
  ```
- **VERIFICATION**: `gcc -c` + `objdump -dr` shows the relocation and `0x0(%rip)`
  placeholder; after linking, objdump shows the resolved disp32.
- **SOURCE**: intel-sdm Vol.1 §3.7.5, §3.7.6; sysv-amd64-abi §4.4; binutils-docs.

## 6. REX prefix roles

- **RULE**: REX = 0100WRXB: W=1 selects 64-bit operands; R extends the ModRM.reg
  field; X extends the SIB index; B extends the ModRM r/m and base (and the B8+rd
  immediate-register forms). REX is required for `r8-r15` and for `spl bpl sil dil`.
- **WHY AI GETS IT WRONG**: thinks REX prefixes can be stacked; forgets REX.B when
  the high register is the r/m operand; assumes `ah/ch/dh/bh` stay reachable when a
  REX prefix is present.
- **CORRECT REASONING**: exactly one REX per instruction. With a REX present, the
  byte registers become `spl/bpl/sil/dil`, making `ah/ch/dh/bh` unreachable. Hand-
  encoded bytes missing REX.B silently target a different register: `8B 00` decodes
  as `mov (%rax),%eax`, not `mov (%r8),%eax`.
- **EXAMPLE** (bad):
  ```asm
  .byte 0x8B, 0x00       # intended mov (%r8),%eax — actually mov (%rax),%eax
  ```
- **COUNTEREXAMPLE** (good):
  ```asm
  movq    %r9, %r8       # assembler emits 4D 89 C8 (REX.W+R+B)
  .byte   0x41, 0x8B, 0x00   # hand-encoded mov (%r8),%eax (REX.B only)
  ```
- **VERIFICATION**: objdump -d prints the prefixes (`4d 89 c8`, `41 8b 00`,
  `8b 00`); compare with the register names in the mnemonic.
- **SOURCE**: intel-sdm Vol.2 (REX); amd64-apm Vol.1 §1.5.1, Vol.2 (REX); agner-fog.

## 7. Canonical 48-bit addresses

- **RULE**: in 64-bit mode only canonical addresses are valid: bits 63:48 must be
  copies of bit 47 (sign extension of the low 48 bits). User range
  0x0–0x00007FFF_FFFFFFFF; kernel range 0xFFFF8000_00000000–0xFFFF_FFFF_FFFF_FFFF.
  Any other address faults with #GP on access.
- **WHY AI GETS IT WRONG**: "64-bit means 2^64 addressable"; assumes high addresses
  like 0x0000800000000000 are fine because they fit in 64 bits.
- **CORRECT REASONING**: the machine implements 48 (or 57 with LA57) virtual bits;
  bit 47 is the virtual-address sign bit. The non-canonical value
  0x0000800000000000 has bit 47 set but upper bits zero — a #GP, typically surfacing
  as SIGSEGV / access violation.
- **EXAMPLE** (bad):
  ```asm
  movabsq $0x0000800000000000, %rax   # assembles fine — faults at runtime
  movq    (%rax), %rcx                # #GP
  ```
- **COUNTEREXAMPLE** (good): rely on `lea sym(%rip)` for addresses, and treat any
  hand-built absolute address as suspect until canonicality is proven.
- **VERIFICATION**: assembling succeeds (`gcc -c` exit 0); running the snippet faults.
  objdump shows `movabs $0x800000000000`.
- **SOURCE**: intel-sdm Vol.3A §3.3.7.1; amd64-apm Vol.2 §5.3.

## 8. Zero vs sign extension

- **RULE**: `movzx`/`movzbl`/`movzwl`/`movzbq`/`movzwq` zero-extend; `movsx`/
  `movsbl`/`movswl`/`movsbq`/`movswq`/`movslq` sign-extend. A 32-bit ALU write also
  zero-extends (rule 2).
- **WHY AI GETS IT WRONG**: `movsbl` is assumed to extend all the way to 64 bits —
  it sign-extends only to 32, and the 32-bit write then zero-extends to 64, so byte
  0xFF yields 0x00000000FFFFFFFF, not -1. Also: `movl` of a negative int32 silently
  produces a huge positive 64-bit value.
- **CORRECT REASONING**: pick the extension by the source signedness AND the
  destination width: byte→64 signed needs `movsbq`; int32→int64 signed needs
  `movslq`; 8/16-bit zero-extension to 64 is satisfied by `movzbl`/`movzwl` into a
  32-bit register (rule 2).
- **EXAMPLE** (bad):
  ```asm
  movl    (%rdi), %eax    # input -1 (0xFFFFFFFF): rax = 0x00000000FFFFFFFF
  movsbl  (%rdi), %eax    # byte 0xFF: rax = 0x00000000FFFFFFFF, not -1
  ```
- **COUNTEREXAMPLE** (good):
  ```asm
  movslq  (%rdi), %rax    # int32 -> int64, sign-extended
  movsbq  (%rdi), %rax    # int8  -> int64, sign-extended
  movzbl  (%rdi), %eax    # uint8 -> zero-extended (to 64 via rule 2)
  ```
- **VERIFICATION**: objdump shows `48 63 07` (movslq) vs `8B 07` (movl) vs `0F BE 07`
  (movsbl) vs `0F B6 07` (movzbl); gdb with input -1/0xFF confirms the values.
- **SOURCE**: intel-sdm Vol.2 (MOVSX, MOVZX); amd64-apm Vol.3.

## 9. Which instructions set flags; signed vs unsigned branches

- **RULE**: add/sub/cmp/test/and/or/xor/shift set flags; `lea`, `mov`, `push`,
  `pop`, `nop` do not; `inc`/`dec` do not touch CF. Branch conditions by flag:
  unsigned `jb/jae`=CF, `jbe/ja`=CF|ZF; signed `jl/jge`=SF vs OF, `jle/jg`=
  ZF|(SF vs OF); `jz/jnz`=ZF; `jo/jc/js` test OF/CF/SF directly.
- **WHY AI GETS IT WRONG**: uses `jb` on signed values (or `jl` on unsigned) because
  "below/less look the same"; assumes `lea` or `mov` updated flags; forgets `cmp` is
  `sub` — `cmp %rsi,%rdi` computes rdi−rsi, so "below" means rdi < rsi unsigned.
- **CORRECT REASONING**: `cmpq %rsi,%rdi; jl .taken` is correct for signed int64;
  `jb` for the same operands tests CF, which is the unsigned result — for rdi=-1,
  rsi=1 the unsigned compare says rdi > rsi, so `jb` falls through where `jl` jumps.
- **EXAMPLE** (bad):
  ```asm
  cmpq    %rsi, %rdi
  jb      .is_neg        # unsigned branch on signed data — wrong for negatives
  ```
- **COUNTEREXAMPLE** (good):
  ```asm
  cmpq    %rsi, %rdi
  jl      .is_neg        # signed less-than (SF != OF)
  ```
- **VERIFICATION**: assemble both; run with rdi=-1, rsi=1 and compare taken/not-taken
  (or single-step gdb, watch ZF/CF/SF/OF after the `cmp`).
- **SOURCE**: intel-sdm Vol.1 §3.6, Vol.2 (per-instruction flag tables); agner-fog.

## 10. Encoding pitfalls: imm32 sign-extension and instruction length

- **RULE**: the compact `C7 /0` (and `83 /0`) immediate forms take a 32-bit
  immediate that is SIGN-EXTENDED to 64 bits. A full 64-bit constant requires the
  B8+imm64 `movabs` form. So `48 C7 C0 FF FF FF FF` means `mov rax,-1`, not
  `mov rax,0xFFFFFFFF`.
- **WHY AI GETS IT WRONG**: reads `48 C7 C0 FF FF FF FF` as "mov rax, 0xffffffff";
  assumes every 32-bit-looking immediate is stored verbatim; estimates instruction
  lengths from the mnemonic instead of the encoding.
- **CORRECT REASONING**: GAS refuses the compact form when the value needs unsigned
  32-bit semantics: `movq $0xFFFFFFFF,%rax` assembles to the 10-byte
  `48 B8 FF FF FF FF 00 00 00 00` (value 0x00000000FFFFFFFF). The compact 7-byte
  form appears for signed-encodable constants: `movq $-1,%rax` → `48 C7 C0 FF FF FF
  FF`; `movq $0x7FFFFFFF,%rbx` → `48 C7 C3 FF FF FF 7F`.
- **EXAMPLE** (bad):
  ```asm
  .byte 0x48, 0xC7, 0xC0, 0xFF, 0xFF, 0xFF, 0xFF   # decodes as mov rax,-1
  ```
- **COUNTEREXAMPLE** (good):
  ```asm
  movq    $-1, %rax           # 48 C7 C0 FF FF FF FF — compact and correct for -1
  movl    $0xFFFFFFFF, %eax   # B8 FF FF FF FF — zero-extends to 0x00000000FFFFFFFF
  movabsq $0xFFFFFFFF, %rax   # 48 B8 ... — exact 64-bit constant, 10 bytes
  ```
- **VERIFICATION**: objdump -d prints `mov $0xffffffffffffffff,%rax` for the C7 form
  and `movabs $0xffffffff,%rax` for the B8 form; compare byte lengths.
- **SOURCE**: intel-sdm Vol.2 (MOV, C7 /0); agner-fog instruction tables; binutils-docs.

## Quick reference table

| Topic | Rule in one line |
|---|---|
| Widths | `.b` 8, `.w` 16, `.l` 32, `.q` 64; default operand size is 32-bit |
| 32-bit writes | zero-extend to 64; 8/16-bit writes keep upper bits |
| Address forms | disp / base+disp / base+idx*scale+disp / idx*scale+disp / RIP-rel |
| Scale | 1, 2, 4, 8 only; `rsp` cannot index; one base, one index |
| RIP-relative | disp32 from the next RIP; PIC-safe; linker-resolved |
| REX | 0100WRXB; needed for r8-r15 and 64-bit operands; one per instruction |
| Canonical | bits 63:48 = bit 47; else #GP; user top 0x00007FFF_FFFFFFFF |
| Extension | `movslq/movsbq` sign; `movzbl/movl` zero; `movsbl` only reaches 32 |
| Flags | lea/mov/push don't set; inc/dec skip CF; jb=CF, jl=SF≠OF |
| Imm32 | C7/83 sign-extend imm32; full 64-bit constants need movabs (B8) |
