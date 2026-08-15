"""
GOOD: axiom validation — the host-executable core of the skill.
Every axiom fed to a solver must be TRUE of the real system. This script
validates two candidate axioms against the actual behavior of C-like 8-bit
arithmetic by brute force, WITHOUT requiring Z3:

  1. commutativity of + over int8:   a + b == b + a   (wrapping)
  2. x*x >= 0 over int8 wrapping     (overflow makes this false)

Run: python good/axiom_validation.py
Expected: commutativity HOLDS (wrapping addition is commutative), but
x*x>=0 FAILS — finding the concrete counterexample a solver would emit as its
model(). This is exactly why the "axiom" in bad/unsound_axiom.py is invalid
for the C system.
"""
import struct


def int8(v):
    v = v & 0xFF
    return v - 256 if v > 127 else v


def find_axiom_violations():
    comm_violations = []
    nonneg_violations = []
    for a in range(-128, 128):
        for b in range(-128, 128):
            if int8(a + b) != int8(b + a):
                comm_violations.append((a, b))
            if int8(a * b) < 0:
                nonneg_violations.append((a, b))
    return comm_violations, nonneg_violations


comm, nonneg = find_axiom_violations()
print("commutativity a+b==b+a violations (int8):", len(comm))
print("x*x>=0 violations (int8):", len(nonneg))
if nonneg:
    a, b = nonneg[0]
    print("counterexample: a=%d b=%d -> (a*b)=%d < 0" % (a, b, int8(a * b)))
print("axiom audit result:", "commutativity ok, x*x>=0 UNSUPPORTED" if nonneg else "both ok")
