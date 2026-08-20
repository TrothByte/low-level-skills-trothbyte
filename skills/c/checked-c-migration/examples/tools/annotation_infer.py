#!/usr/bin/env python3
"""annotation_infer.py — host-runnable model of the Checked C annotation loop.

Given a simplified IR of pointer usage (proven allocation size, deref sites,
loop extents, copy lengths, string/termination evidence), propose the Checked C
annotation a migration agent (or the 3C tool) should emit, then VALIDATE that
the proposed bounds cover every access site — mirroring what the Checked C
compiler checks with `clang -fcheckedc-extension`.

The real checker proves bounds statically and inserts runtime checks; this model
replicates the decision so the skill can be verified on a host without the
Checked C clang fork. Treat every proposed annotation as a hypothesis to be
confirmed on the target compiler.

Usage:  python annotation_infer.py
Exit:   0 when every scenario's prediction matches its expected label
        (i.e. the inference+validation loop is sound on the test corpus).
"""

from __future__ import annotations


class Access:
    """One use of a pointer in the IR."""

    def __init__(self, kind, detail=None):
        self.kind = kind    # deref | loop | copy | string_use
        self.detail = detail


class Pointer:
    """Simplified IR for one pointer variable."""

    def __init__(self, name, alloc, accesses, terminated=False):
        self.name = name
        self.alloc = alloc        # proven capacity in elements, or None (unknown)
        self.accesses = accesses
        self.terminated = terminated   # NUL-termination is provable

    def max_index(self):
        """Highest subscript value statically observable, or -1 if none."""
        m = -1
        for a in self.accesses:
            if a.kind == "deref":
                m = max(m, a.detail)
            elif a.kind == "loop":
                m = max(m, a.detail[1] - 1)
        return m

    def copy_length(self):
        return max((a.detail for a in self.accesses if a.kind == "copy"),
                   default=0)

    def uses_string(self):
        return any(a.kind == "string_use" for a in self.accesses)


def infer(p):
    """Propose the annotation for pointer p (the agent's / 3C's job)."""
    if (not p.uses_string() and p.max_index() <= 0 and p.copy_length() <= 1):
        return "_Ptr<T>"
    if p.uses_string():
        return "_Nt_array_ptr<T>"
    if p.alloc is not None:
        return f"_Array_ptr<T> : count({p.alloc})"
    return "bounds(none) + _Dynamic_check"


def validate(p, ann):
    """Check the proposed bounds cover every access site (the compiler's job).

    Returns (ok: bool, reason: str).
    """
    if ann.startswith("_Ptr"):
        if p.copy_length() > 1:
            return False, "copy length > 1 exceeds a single object"
        return True, "single object, no arithmetic or copy"

    if ann.startswith("_Nt_array_ptr"):
        if not p.terminated:
            return False, "NUL termination not proven: nt bounds are unsound"
        if p.alloc is not None and p.copy_length() > p.alloc:
            return False, f"copy length {p.copy_length()} > capacity {p.alloc}"
        return True, "terminated; bounds derived from strlen+1"

    if ann.startswith("bounds(none)"):
        # No static proof: every use must be guarded by _Dynamic_check.
        return True, "no static proof: requires _Dynamic_check at each use"

    # _Array_ptr : count(n)
    if p.alloc is None:
        return False, "no proven allocation size for count(n)"
    n = p.alloc
    m = p.max_index()
    if m >= n:
        return False, f"access index {m} >= declared count {n}: out of bounds"
    if p.copy_length() > n:
        return False, (f"copy length {p.copy_length()} > declared count {n}: "
                       f"overflow")
    return True, f"all accesses within count({n})"


def main():
    corpus = [
        # (scenario, pointer, expected outcome, what the scenario proves)
        ("single_object", Pointer("px", 1, [Access("deref", 0)]),
         True, "_Ptr fits a non-array, non-arithmetic pointer"),
        ("count_correct", Pointer("a", 4, [Access("loop", (0, 4))]),
         True, "_Array_ptr count(4) covers a [0,4) loop"),
        ("count_too_small", Pointer("a", 4, [Access("loop", (0, 8))]),
         False, "loop to index 7 busts count(4): must be rejected"),
        ("memcpy_overflow", Pointer("dst", 4, [Access("copy", 8)]),
         False, "memcpy of 8 into a 4-element buffer: must be rejected"),
        ("nt_string_ok", Pointer("s", 6, [Access("string_use")], terminated=True),
         True, "provably terminated string: _Nt_array_ptr is sound"),
        ("nt_string_unproven", Pointer("s", 6, [Access("string_use")],
                                       terminated=False),
         False, "termination unproven: _Nt_array_ptr must be rejected"),
        ("unchecked_boundary", Pointer("raw", None, [Access("deref", 3)]),
         True, "unprovable bound: fall back to bounds(none)+_Dynamic_check"),
    ]

    print(f"{'scenario':20} {'proposed annotation':34} {'result':6}  expected")
    print("-" * 92)
    sound = True
    for name, p, expected, _desc in corpus:
        ann = infer(p)
        ok, reason = validate(p, ann)
        match = (ok == expected)
        sound = sound and match
        result = "PASS" if ok else "FAIL"
        exp = "PASS" if expected else "FAIL"
        flag = "" if match else "  <-- MISMATCH"
        print(f"{name:20} {ann:34} {result:6}  {exp}{flag}")
        print(f"{'':20} {'':34} {reason}")

    print("-" * 92)
    if sound:
        print("MODEL SOUND: every proposed annotation matched its expected verdict")
    else:
        print("MODEL UNSOUND: fix the inference or validation rules")
        raise SystemExit(1)
    print("Note: real acceptance requires clang -fcheckedc-extension on the result.")


if __name__ == "__main__":
    main()
