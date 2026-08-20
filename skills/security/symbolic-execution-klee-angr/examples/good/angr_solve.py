"""TARGET-ONLY: recover the crash input of angr_target.exe with angr.

Requires: angr installed (`pip install angr`). On this repository host
the install attempt outcome is recorded in evals/README.md; this script
runs where angr is importable.

Setup and run:

  gcc -g -O0 angr_target.c -o angr_target.exe
  python angr_solve.py

Approach: analysis starts at analyze_me() with the g_input global made
symbolic. Each character comparison forks the state (accumulating path
constraints), and the explorer finds the state that reaches crash_me;
solving its constraints yields the concrete input. Starting at
analyze_me (instead of the PE entry) skips the MinGW CRT startup and
argv plumbing — the "model the environment, do not execute the CRT"
rule.
"""
import os
import sys

import angr
import claripy

HERE = os.path.dirname(os.path.abspath(__file__))
TARGET = os.path.join(HERE, "angr_target.exe")

MAGIC_NEEDED = b"VULN"


def main() -> int:
    proj = angr.Project(TARGET, auto_load_libs=False)

    analyze = proj.loader.find_symbol("analyze_me")
    crash = proj.loader.find_symbol("crash_me")
    g_input = proj.loader.find_symbol("g_input")
    for name, sym in (("analyze_me", analyze), ("crash_me", crash),
                      ("g_input", g_input)):
        if sym is None:
            print(f"FAIL: symbol '{name}' not found; rebuild with -g")
            return 1

    arg = claripy.BVS("magic", 16 * 8)  # 16 symbolic bytes in g_input
    state = proj.factory.blank_state(
        addr=analyze.rebased_addr,
        add_options={
            angr.options.ZERO_FILL_UNCONSTRAINED_REGISTERS,
            angr.options.ZERO_FILL_UNCONSTRAINED_MEMORY,
        },
    )
    state.memory.store(g_input.rebased_addr, arg)

    sm = proj.factory.simulation_manager(state)
    found = sm.explore(find=crash.rebased_addr)

    if not found.found:
        print("FAIL: no state reached crash_me")
        return 1

    concrete = found.found[0].solver.eval(arg, cast_to=bytes)
    prefix = bytes(concrete[:4])
    if prefix == MAGIC_NEEDED:
        print("PASS: reached crash_me with input:", concrete)
        return 0
    print("FAIL: reached crash_me but input has unexpected prefix:", prefix)
    return 1


if __name__ == "__main__":
    sys.exit(main())
