"""
GOOD: sound SMT usage — safety encoded as negation, model() read on sat.
Property: for the real 8-bit C uint8_t computation, x*x can overflow.
Encoding: assert Not(x*x >= 0 over BitVec8) — `unsat` would prove the
property; `sat` (expected) means a counterexample exists, and we extract it
from model() and reproduce it in the real arithmetic.

Z3 is not installed on this host; the SAME procedure is run without Z3 by
good/axiom_validation.py, which brute-forces the 8-bit space to find the
counterexample that Z3's model() would report.
"""
from z3 import BitVec, Not, Solver

x = BitVec('x', 8)
s = Solver()
# safety property P: x*x >= 0  (in 8-bit wrapping arithmetic)
s.add(Not(x * x >= 0))
r = s.check()
if r == "sat":
    m = s.model()
    print("counterexample: x =", m[x], "-> x*x wraps below zero")
    # reproduce with real arithmetic:
    val = m[x].as_long()
    wrapped = (val * val) & 0xFF
    print("repro: (%d*%d) & 0xFF = %d  (negative in int8 view)" %
          (val, val, wrapped if wrapped > 127 else wrapped))
else:
    print("property holds (unsat of negation)")
