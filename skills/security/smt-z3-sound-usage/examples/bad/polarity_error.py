"""
BAD: intentionally incorrect — polarity error: reads `sat` as success.
The property "x*x >= 0" is encoded directly (not as a negation), and `sat` is
reported as "solver says it holds". Z3 over the INT theory actually returns
unsat for x*x < 0, but if x is a REAL or the model is the BitVec 16 theory,
x*x can be negative (overflow) — and in all cases the encoding must assert
Not(P) for a safety query.

Host note: Z3 is not installed; reviewed as the specimen.
"""
from z3 import BitVec, Solver

x = BitVec('x', 8)  # 8-bit: x*x wraps around
s = Solver()
s.add(x * x < 0)
if s.check() == "unsat":
    print("proved: x*x >= 0")   # WRONG polarity reading — unsat of the BAD
    # clause means no 8-bit x gives x*x<0, which is the actual finding
