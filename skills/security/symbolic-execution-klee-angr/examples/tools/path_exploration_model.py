"""Path-exploration model for symbolic execution (z3-free, python 3.11).

A tiny interval-based interpreter that mirrors the core of KLEE/angr
symbolic execution:

  * "symbolic"  declares a symbolic variable with an integer domain
    (the interval analogue of klee_make_symbolic / claripy.BVS).
  * "branch"    forks the state: one child per outcome, each with the
    variable's interval narrowed by the path condition. This is state
    forking exactly as KLEE forks at a conditional on a symbolic value.
  * "assume"    restricts the domain and drops states that contradict
    the assumption (klee_assume).
  * "assert"    checks a property against the state's interval; a
    violation produces a concrete witness input.
  * "target"    records that a program line was reached, with the
    concrete input that reaches it.
  * max_depth bounds forking, exactly like --max-depth / --max-time.

Run:  python path_exploration_model.py

Scenarios (each must print PASS):

  S1  A buggy bounds check: symbolic index in [0,15] is used to index an
      8-entry table; the guard assert fails for idx in [8,15]. Bounded
      exploration finds the violating concrete input (idx = 8) — the KLEE
      .err / angr found-state analogue, plus a concrete replay check.
  S2  Path explosion: 12 independent symbolic variables, 12 branches
      (2^12 = 4096 paths). The full run explores all 4096 states and
      reaches the target only on the all-zero path; the bounded run
      (max_depth=6) terminates early and cannot reach the target — the
      reason tools need depth/time bounds.

No external dependencies; output is deterministic.
"""


class Interval:
    """Disjoint integer interval [lo, hi] (inclusive)."""

    __slots__ = ("lo", "hi")

    def __init__(self, lo, hi):
        self.lo, self.hi = lo, hi

    def __repr__(self):
        return f"[{self.lo},{self.hi}]"


def intersect(ivals, lo, hi):
    """Intersect a sorted disjoint interval list with [lo, hi]."""
    out = []
    for iv in ivals:
        nlo, nhi = max(iv.lo, lo), min(iv.hi, hi)
        if nlo <= nhi:
            out.append(Interval(nlo, nhi))
    return out


class State:
    """A symbolic execution state: variables -> interval sets, path
    constraints (display), depth (number of forks taken), pc (next
    instruction index)."""

    __slots__ = ("vars", "constraints", "depth", "pc")

    def __init__(self, pc=0):
        self.vars = {}
        self.constraints = []
        self.depth = 0
        self.pc = pc

    def clone(self):
        s = State(pc=self.pc)
        s.vars = {k: list(v) for k, v in self.vars.items()}
        s.constraints = list(self.constraints)
        s.depth = self.depth
        return s

    def witness(self):
        """Smallest concrete value of each symbolic variable."""
        return {name: ivals[0].lo for name, ivals in sorted(self.vars.items())}


REL_NEG = {"<": ">=", ">=": "<", "==": "!=", "!=": "=="}


def restrict(ivals, rel, val):
    """Interval set where (var rel val) holds."""
    if rel == "<":
        return intersect(ivals, -1 << 60, val - 1)
    if rel == ">=":
        return intersect(ivals, val, 1 << 60)
    if rel == "==":
        return intersect(ivals, val, val)
    # rel == "!=" : drop val from the set
    out = []
    for iv in ivals:
        if iv.lo < val:
            out.append(Interval(iv.lo, min(iv.hi, val - 1)))
        if iv.hi > val:
            out.append(Interval(max(iv.lo, val + 1), iv.hi))
    return out


def violates(ivals, rel, val):
    """True if some value in the set violates (var rel val)."""
    return bool(restrict(ivals, REL_NEG[rel], val))


def witness_neg(ivals, rel, val):
    """A concrete value in the set that violates (var rel val)."""
    neg = restrict(ivals, REL_NEG[rel], val)
    if not neg:
        return None
    return neg[0].lo


def fork(st, var, rel, val):
    """Branch on (var rel val): return the two child states."""
    true_s, false_s = st.clone(), st.clone()
    true_s.depth += 1
    false_s.depth += 1
    true_s.vars[var] = restrict(st.vars[var], rel, val)
    false_s.vars[var] = restrict(st.vars[var], REL_NEG[rel], val)
    true_s.constraints.append((var, rel, val))
    false_s.constraints.append((var, REL_NEG[rel], val))
    return true_s, false_s


def interpret(program, max_depth=None, max_states=None):
    """BFS over forked states, each with its own program counter.

    Returns (targets, violations, forks, completed_states,
             depth_bound_hit, state_limit_hit). A branch forks the state
    into two children that resume at the instruction after the branch;
    the parent never continues (exact symbolic-execution semantics).
    """
    queue = [State()]
    targets = []
    violations = []
    forks = 0
    completed = 0
    depth_bound = max_depth is not None
    state_limit = False
    while queue:
        if max_states is not None and forks >= max_states:
            state_limit = True
            break
        st = queue.pop(0)
        finished = True
        while st.pc < len(program):
            ins = program[st.pc]
            op = ins[0]
            if op == "symbolic":
                _, var, lo, hi = ins
                if var not in st.vars:
                    st.vars[var] = [Interval(lo, hi)]
                st.pc += 1
            elif op == "assume":
                _, var, rel, val, *note = ins
                narrowed = restrict(st.vars[var], rel, val)
                if not narrowed:
                    finished = False  # assumption unsatisfiable: drop
                    break
                st.vars[var] = narrowed
                st.constraints.append((var, rel, val))
                st.pc += 1
            elif op == "branch":
                _, var, rel, val, *note = ins
                forks += 1
                for child in fork(st, var, rel, val):
                    child.pc = st.pc + 1
                    if depth_bound and child.depth >= max_depth:
                        continue  # depth-bounded: do not explore further
                    queue.append(child)
                finished = False  # parent does not continue past the fork
                break
            elif op == "assert":
                _, var, rel, val, msg = ins
                if violates(st.vars[var], rel, val):
                    w = witness_neg(st.vars[var], rel, val)
                    found = any(
                        m == msg and w == ew for m, ew, _ in violations
                    )
                    if not found:
                        violations.append((msg, w, list(st.constraints)))
                st.pc += 1
            elif op == "target":
                _, name = ins
                if not any(n == name for n, _, _ in targets):
                    targets.append((name, st.witness(), list(st.constraints)))
                st.pc += 1
            else:
                raise ValueError("unknown op: " + op)
        if finished:
            completed += 1
    return targets, violations, forks, completed, depth_bound, state_limit


def concrete_replay(program, witness):
    """Run the program concretely with a witness input and return the set
    of targets reached and assert violations observed. This is the
    concrete-replay gate: a solver finding only counts if the concrete
    run reproduces it."""
    vals = dict(witness)
    targets = []
    violations = []
    for ins in program:
        op = ins[0]
        if op == "assert":
            _, var, rel, val, msg = ins
            x = vals[var]
            ok = {"<": x < val, ">=": x >= val,
                  "==": x == val, "!=": x != val}[rel]
            if not ok:
                violations.append(msg)
        elif op == "target":
            targets.append(ins[1])
    return targets, violations


def scenario1():
    """Bounded exploration finds the OOB input (KLEE/angr finding +
    concrete replay)."""
    prog = [
        ("symbolic", "idx", 0, 255),
        ("assume", "idx", ">=", 0),
        ("assume", "idx", "<", 16),       # too-loose bound (should be 8)
        ("branch", "idx", "<", 8, "bounds check"),   # fork at the guard
        # both children continue here; child [8,15] violates the assert
        ("assert", "idx", "<", 8, "table[8] access must stay in bounds"),
        ("target", "after_check"),
    ]
    targets, violations, forks, completed, depth_hit, limit_hit = interpret(
        prog, max_depth=8
    )
    assert forks == 1, f"S1 expected one fork point, got {forks}"
    assert completed == 2, f"S1 expected both children to finish, got {completed}"
    assert len(violations) == 1, f"S1 expected exactly one violation, got {violations}"
    assert violations[0][1] == 8, f"S1 expected witness idx=8, got {violations[0]}"
    # concrete replay with the found input must reproduce the violation
    rep_targets, rep_violations = concrete_replay(prog, {"idx": 8})
    assert "table[8] access must stay in bounds" in rep_violations
    print("PASS S1: bounded exploration found violating input idx=%d "
          "(fork points=%d, completed states=%d) and concrete replay "
          "reproduces the out-of-bounds access"
          % (violations[0][1], forks, completed))


def scenario2():
    """Path explosion: 12 independent branches = 2^12 = 4096 states.
    The bounded run (max_depth=6) terminates early and cannot reach the
    target; the full run explores all 4096 states and reaches it."""
    prog = []
    for i in range(12):
        prog.append(("symbolic", f"v{i}", 0, 1))
    for i in range(12):
        prog.append(("branch", f"v{i}", "==", 0, f"v{i} must be 0"))
    prog.append(("target", "all_zero_path"))

    t_b, _, forks_b, comp_b, depth_hit_b, _ = interpret(prog, max_depth=6)
    assert depth_hit_b, "S2 bounded run must hit the depth bound"
    assert not t_b, "S2 bounded run must NOT reach the target (depth 12 needed)"
    assert comp_b == 0, "S2 bounded run: no state may complete the program"
    assert forks_b < 4095, "S2 bounded run must fork far fewer than 4095 times"

    t_f, _, forks_f, comp_f, _, _ = interpret(prog)
    assert forks_f == 4095, f"S2 full run must fork 2^12 - 1 = 4095 times, got {forks_f}"
    assert comp_f == 4096, f"S2 full run must complete 2^12 = 4096 states, got {comp_f}"
    assert len(t_f) == 1 and t_f[0][0] == "all_zero_path"
    witness = t_f[0][1]
    assert all(witness[f"v{i}"] == 0 for i in range(12)), witness
    rep_targets, _ = concrete_replay(prog, witness)
    assert "all_zero_path" in rep_targets

    print(f"PASS S2: unbounded fanout is exponential (full run: "
          f"{forks_f} forks -> {comp_f} terminal states; target reached "
          f"only via the all-zero input); bounded run (max_depth=6) "
          f"forked only {forks_b} times, terminated early and did NOT "
          f"reach the target — the path-explosion lesson")


def main():
    scenario1()
    scenario2()
    print("PASS: all scenarios")


if __name__ == "__main__":
    main()
