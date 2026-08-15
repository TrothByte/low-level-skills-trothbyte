"""
BAD: intentionally incorrect — unsound axiom passed to Z3.
The "axiom" ForAll(a,b) a+b==b+a is TRUE over unbounded mathematical integers
but FALSE over C `int` under signed overflow (UB / wraps in two's complement
implementations). A proof built on this axiom does not hold for the C program
it is meant to validate, yet the agent reports "Z3 proved it".

Toolchain note: Z3 is not installed on this host; this file is the reviewed
specimen. The axiom's falseness in C is demonstrated by host-run
good/axiom_validation.py.
"""
from z3 import Int, ForAll, Solver, sat

a = Int('a')
b = Int('b')
s = Solver()

# the unsound axiom: claims commutativity of + over all integers
s.add(ForAll([a, b], a + b == b + a))

# "prove" that (a+b)+c == a+(b+c) — true in the theory, but the theory is
# not C: in C, signed overflow is undefined behavior, so associativity and
# commutativity do not hold in the real system.
x = Int('x')
y = Int('y')
z = Int('z')
s.add(Not((x + y) + z == x + (y + z)))
print("check:", s.check())  # unsat -> "proved" — but only in the flawed theory
