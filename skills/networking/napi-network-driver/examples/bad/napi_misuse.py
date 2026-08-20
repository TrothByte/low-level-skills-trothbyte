#!/usr/bin/env python3
"""Host-runnable model of BUGGY NAPI drivers, with a checker that flags the
misuse and emits a diagnostic for each one.

Each bug below corresponds to a real class of driver defect:

  BUG 1 — napi_complete() called when work_done == budget (budget exhausted):
          the ring is still full, the instance must stay scheduled. Calling
          napi_complete clears the scheduled state and re-enables the IRQ,
          which immediately re-fires: interrupt storm, device wedged.

  BUG 2 — heavy work inside the IRQ handler: packets are processed in IRQ
          context instead of being deferred to the poll() round. This defeats
          NAPI (high IRQ cost, no batching, softirq starvation).

  BUG 3 — napi_schedule() without masking the queue IRQ: the IRQ re-enters
          while the instance is already scheduled. Repeated re-entry = storm.

The checker runs each buggy driver through the same lifecycle harness as the
good model and emits a DIAGNOSTIC line for every violation it observes. The
expected diagnostics are compared against what fired; exit code 0 only when
all three bugs are caught with the right message.

Run: python examples/bad/napi_misuse.py
"""

BUDGET = 64

EXPECTED_DIAGNOSTICS = {
    "complete_on_budget": "DIAGNOSTIC: napi_complete with work_done == budget; ring not drained, instance must stay scheduled",
    "heavy_irq": "DIAGNOSTIC: packet processing inside IRQ handler defeats NAPI",
    "irq_not_masked": "DIAGNOSTIC: napi_schedule without masking the queue IRQ (re-entrancy)",
}


class BuggyNapi:
    """Fake napi_struct that records the driver's calls."""

    def __init__(self, budget=BUDGET):
        self.budget = budget
        self.state = "IDLE"
        self.irq_enabled = True
        self.ring = 0
        self.irq_fires = 0              # unmasked IRQ re-entries
        self.poll_calls = 0

    def napi_schedule(self):
        if self.state == "SCHEDULED":
            return False
        self.state = "SCHEDULED"
        return True

    def napi_complete(self):
        self.state = "IDLE"

    def napi_complete_done(self, work_done):
        self.state = "IDLE"
        return True


def driver_bug1_complete_on_budget(napi):
    """BUG 1: always call napi_complete + re-enable IRQ, even at budget."""
    work_done = 0
    while work_done < napi.budget and napi.ring > 0:
        napi.ring -= 1
        work_done += 1
    # WRONG: unconditional complete, even when work_done == budget
    napi.napi_complete()
    napi.irq_enabled = True
    return work_done


def irq_handler_bug1(napi):
    napi.ring = 200
    if napi.napi_schedule():
        napi.irq_enabled = False
        return "scheduled"
    return "dropped"


def driver_bug2_heavy_irq(napi):
    """BUG 2: the IRQ handler processes packets itself (see
    irq_handler_bug2) — poll() is never scheduled. Model: IRQ fires, handler
    drains the ring in place, no schedule, no poll."""
    processed = napi.ring
    napi.ring = 0
    napi.poll_calls += 0
    return processed


def irq_handler_bug2(napi):
    napi.ring = 200
    processed = driver_bug2_heavy_irq(napi)   # work in IRQ context
    return "processed_in_irq:%d" % processed


def driver_bug3_irq_not_masked(napi):
    """BUG 3: proper poll body, but the IRQ path never masks the queue IRQ."""
    work_done = 0
    while work_done < napi.budget and napi.ring > 0:
        napi.ring -= 1
        work_done += 1
    if work_done < napi.budget:
        if napi.napi_complete_done(work_done):
            napi.irq_enabled = True
    return work_done


def irq_handler_bug3(napi):
    napi.ring = 200
    # WRONG: schedule but leave the queue IRQ unmasked
    return "scheduled_no_mask" if napi.napi_schedule() else "dropped"


# -- checker ---------------------------------------------------------------

def simulate(irq_handler, poll_driver, unmasked_reentry=False):
    """Run the lifecycle; return (diagnostics, telemetry)."""
    napi = BuggyNapi()
    diagnostics = []

    if irq_handler is irq_handler_bug2:
        # BUG 2 manifest: work done in the IRQ handler before any schedule.
        ret = irq_handler(napi)
        diagnostics.append(
            "DIAGNOSTIC: packet processing inside IRQ handler defeats NAPI"
            if "processed_in_irq" in ret else "MISSED: heavy work in IRQ handler"
        )
        return diagnostics, {"irq_ret": ret, "poll_calls": napi.poll_calls}

    ret = irq_handler(napi)
    # BUG 3 check: if scheduling happened without masking, and further IRQs
    # arrive, the handler is re-entered while still SCHEDULED.
    if unmasked_reentry and napi.state == "SCHEDULED" and napi.irq_enabled:
        napi.irq_fires += 1
        irq_handler(napi)          # re-entry
        napi.irq_fires += 1
        irq_handler(napi)          # and again
        diagnostics.append(
            "DIAGNOSTIC: napi_schedule without masking the queue IRQ (re-entrancy)"
            if napi.irq_fires > 1 else "MISSED: re-entrancy"
        )

    # softirq: at most one poll round for these bug drivers
    napi.poll_calls += 1
    work_done = poll_driver(napi)

    if poll_driver is driver_bug1_complete_on_budget:
        if work_done == BUDGET and napi.state == "IDLE" and napi.ring > 0:
            diagnostics.append(
                "DIAGNOSTIC: napi_complete with work_done == budget; "
                "ring not drained, instance must stay scheduled"
            )
        else:
            diagnostics.append("MISSED: complete-on-budget not detected")

    return diagnostics, {
        "work_done": work_done,
        "state": napi.state,
        "irq_fires": napi.irq_fires,
        "ring": napi.ring,
    }


def main():
    cases = [
        ("complete_on_budget", irq_handler_bug1, driver_bug1_complete_on_budget, False),
        ("heavy_irq", irq_handler_bug2, driver_bug2_heavy_irq, False),
        ("irq_not_masked", irq_handler_bug3, driver_bug3_irq_not_masked, True),
    ]

    ok = True
    for name, irq_h, poll_d, reentry in cases:
        diagnostics, telemetry = simulate(irq_h, poll_d, reentry)
        fired = [d for d in diagnostics]
        expected = EXPECTED_DIAGNOSTICS[name]
        hit = expected in fired
        ok = ok and hit
        status = "PASS" if hit else "FAIL"
        print(f"[{status}] bug={name} telemetry={telemetry}")
        for d in fired:
            print(f"       {d}")

    print(f"\nNAPI misuse checker: {len(cases)} bugs checked, "
          f"{'ALL FLAGGED' if ok else 'SOME MISSED'}")
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
