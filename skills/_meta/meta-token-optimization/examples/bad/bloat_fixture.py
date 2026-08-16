# BAD: bloat_fixture.py
# intentionally incorrect
"""
Generates a bloated SKILL.md whose activation cost exceeds the 2000-token
gate. The fixture demonstrates the failure mode this skill exists to prevent:
the body embeds full reference tables, duplicates the description into every
section, and repeats knowledge that belongs in references/ or another skill.
"""
# intentionally incorrect

import textwrap


def bloated_skill():
    dup = textwrap.dedent("""
        ## Table of x86-64 calling convention registers (REPEATED 20x)
        | Reg | Role |
        |---|---|
        | rdi | arg1 |
        | rsi | arg2 |
        | rdx | arg3 |
        | rcx | arg4 |
        | r8  | arg5 |
        | r9  | arg6 |
        | XMM0-7 | float args |
        | rax | vararg count |
        | rsp | stack pointer |
        | rbp/rbx/r12-15 | callee-saved |
        """)
    head = textwrap.dedent("""\
        ---
        name: bloated-example
        description: Use when the agent needs to know everything about the
          x86-64 calling convention, ABI layout, red zone, stack alignment,
          variadic arguments, vector registers, XMM spills, callee-saved
          registers, stack unwinding metadata, and the SysV psABI in full
          detail at all times, plus the Windows x64 differences, plus the
          AAPCS64 differences, plus every optimization flag that affects
          them, duplicated across every section of this file.
        ---
        # Bloated Example Skill
        """)
    return head + dup * 40 + textwrap.dedent("""
        ## When to use
        Use whenever you touch any code that involves registers, the stack,
        the calling convention, or the ABI, and also this duplicated list:
        rdi rsi rdx rcx r8 r9 XMM0-7 red zone 128 bytes stack alignment 16
        bytes callee-saved rbx rbp r12-15 varargs AL count stack arguments.

        ## When not to use
        Do not use it only when the code has nothing to do with registers,
        stack, ABI, alignment, spilling, or red zones; otherwise the same
        table above applies again (rdi rsi rdx rcx r8 r9 ...).

        ## How to reason correctly
        1. First repeat the register table (rdi rsi rdx rcx r8 r9).
        2. Then consider the red zone and 16-byte stack alignment.
        3. Then duplicate the table once more for emphasis.
        """)


if __name__ == "__main__":
    out = bloated_skill()
    print(out)
    print(f"# estimated tokens: {round(len(out)/3.5*0.6 + len(out.split())/1.3*0.4)}")
