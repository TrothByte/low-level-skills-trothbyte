#!/usr/bin/env python3
"""wcet_budget.py - RMS utilization bound + response-time analysis model.

Hard real-time schedulability check for fixed-priority preemptive
scheduling. Input: per-task worst-case execution time C, period T and
deadline D (period == deadline by default). Output: utilization per task,
total utilization vs the Liu & Layland RMS bound

    U_rms = n * (2^(1/n) - 1)

and exact worst-case response times from the Audsley fixed-point iteration

    R_i = C_i + sum_{j in hp(i)} ceil(R_i / T_j) * C_j

The RMS bound is sufficient, not necessary; the response-time test decides.
A "schedulable" verdict is only as good as the WCET inputs, which must come
from a WCET analyzer or cycle-counter measurement of the MAX, never an
average.

Usage: python wcet_budget.py [--scenario schedulable|overloaded]
Exit:  0 = schedulable | 1 = overloaded / deadline missed
"""

import math
import sys


def rms_bound(n):
    return n * (2.0 ** (1.0 / n) - 1.0)


def response_time(task, higher):
    """Audsley fixed-point iteration; None means the deadline is missed."""
    c = task["c"]
    d = task["d"]
    r = c
    for _ in range(100000):
        new_r = c + sum(math.ceil(r / h["t"]) * h["c"] for h in higher)
        if new_r == r:
            return r
        if new_r > d:
            return None
        r = new_r
    return None


SCENARIOS = {
    "schedulable": {
        "note": "control loop + telemetry + watchdog; utilization far below the RMS bound, RTA converges",
        "tasks": [
            {"name": "control", "c": 2.0, "t": 10.0, "d": 10.0},
            {"name": "telemetry", "c": 1.0, "t": 40.0, "d": 40.0},
            {"name": "watchdog", "c": 0.5, "t": 50.0, "d": 50.0},
        ],
    },
    "overloaded": {
        "note": "periods cut without removing work; U > 1.0, response times diverge",
        "tasks": [
            {"name": "control", "c": 9.0, "t": 10.0, "d": 10.0},
            {"name": "telemetry", "c": 6.0, "t": 30.0, "d": 30.0},
            {"name": "watchdog", "c": 2.0, "t": 40.0, "d": 40.0},
        ],
    },
}


def run(scenario):
    info = SCENARIOS[scenario]
    tasks = info["tasks"]
    order = sorted(tasks, key=lambda t: (t["t"], -t["c"]))

    print("wcet_budget.py - fixed-priority schedulability model")
    print("Scenario: %s" % scenario)
    print("Note: %s\n" % info["note"])

    print("RMS priority order: %s\n" % " > ".join(t["name"] for t in order))

    print("%-12s %8s %8s %8s %8s %10s %10s %8s"
          % ("Task", "C(ms)", "T(ms)", "D(ms)", "U", "R(ms)", "Deadline", "Verdict"))
    util = 0.0
    ok_all = True
    for i, t in enumerate(order):
        u = t["c"] / t["t"]
        util += u
        r = response_time(t, order[:i])
        if r is None:
            verdict = "MISS"
            ok_all = False
        elif r <= t["d"]:
            verdict = "OK"
        else:
            verdict = "MISS"
            ok_all = False
        print("%-12s %8.1f %8.1f %8.1f %8.3f %10s %10.1f %8s"
              % (t["name"], t["c"], t["t"], t["d"], u,
                 "n/a" if r is None else "%.1f" % r, t["d"], verdict))

    bound = rms_bound(len(order))
    print("\nTotal utilization U = %.3f (RMS bound for n=%d: U_rms = %.3f)"
          % (util, len(order), bound))

    if util > 1.0:
        print("Verdict: OVERLOADED (U > 1.0: the work cannot fit the periods)")
        return 1
    if util <= bound and ok_all:
        print("Verdict: SCHEDULABLE (U <= U_rms and every response time <= deadline)")
        return 0
    if ok_all:
        print("Verdict: SCHEDULABLE by response-time analysis (U exceeds the bound;"
              " RTA still converges within every deadline)")
        return 0
    print("Verdict: UNSCHEDULABLE (a task misses its deadline in the fixed-point analysis)")
    return 1


def main():
    args = sys.argv[1:]
    scenario = "schedulable"
    if args:
        if args[0] == "--scenario" and len(args) > 1:
            scenario = args[1]
        elif args[0] in SCENARIOS:
            scenario = args[0]
        else:
            print("usage: wcet_budget.py [--scenario schedulable|overloaded]")
            return 2
    if scenario not in SCENARIOS:
        print("unknown scenario '%s'; choose from: %s"
              % (scenario, ", ".join(sorted(SCENARIOS))))
        return 2
    return run(scenario)


if __name__ == "__main__":
    raise SystemExit(main())
