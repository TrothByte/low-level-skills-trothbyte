#!/usr/bin/env python3
"""Host-runnable state-machine model of the NAPI poll discipline.

Models the Linux NAPI lifecycle: IRQ -> napi_schedule -> NET_RX_SOFTIRQ poll
rounds bounded by the per-round budget. Tracks napi->state (IDLE/SCHEDULED),
queue IRQ on/off, work_done per round, and ring depth.

Rules encoded (from docs.kernel.org/networking/napi.html and net/core/dev.c):

  1. napi_schedule() is a no-op when the instance is already scheduled; it
     does not double-queue on the softnet backlog.
  2. poll() must return the number of packets actually processed, <= budget.
  3. If work_done == budget the instance STAYS scheduled: poll must NOT call
     napi_complete(); the softirq re-runs the poll loop (net_rx_action).
  4. If work_done < budget the instance is done: poll calls
     napi_complete_done(work_done); the return value says whether the queue
     IRQ may be re-enabled.
  5. While scheduled, arriving IRQs are dropped (no re-schedule, no re-entry);
     the driver masks the IRQ when scheduling and unmasks on completion.

Scenario tests:
  (a) heavy load: ring stays deeper than budget -> instance stays SCHEDULED
      across many softirq rounds, IRQ stays masked, only the first IRQ arms it.
  (b) light load: ring drains below budget -> napi_complete_done, state back
      to IDLE, IRQ re-enabled.
  (c) IRQ re-entrancy: an IRQ that arrives while SCHEDULED must not cause a
      second schedule or a re-entry into poll.

Exit code 0 only when all scenarios PASS.
"""

BUDGET = 64


class Napi:
    """Model of one napi_struct instance and its queue IRQ."""

    def __init__(self, budget=BUDGET):
        self.budget = budget
        self.state = "IDLE"            # IDLE | SCHEDULED
        self.irq_enabled = True
        self.poll_calls = 0
        self.irq_arrivals = 0          # IRQs that fired (hardware events)
        self.schedules = 0             # times napi_schedule actually armed us
        self.work_done = 0
        self.completes = 0             # times napi_complete_done was called
        self.ring = 0

    # -- kernel-equivalents -------------------------------------------------

    def napi_schedule(self):
        self.irq_arrivals += 1
        if self.state == "SCHEDULED":
            # napi_schedule is a no-op; NAPI_STATE_SCHED already set.
            return False
        self.state = "SCHEDULED"
        self.schedules += 1
        return True

    def napi_complete_done(self, work_done):
        # Returns True when the queue IRQ may be re-enabled.
        self.completes += 1
        self.state = "IDLE"
        return True

    def irq(self, ring_depth):
        """IRQ handler: arrival, schedule, mask the queue IRQ."""
        self.ring = ring_depth
        if self.napi_schedule():
            self.irq_enabled = False          # mask queue IRQ
            return "scheduled"
        return "dropped"

    def poll_round(self, driver):
        """One poll() invocation under NET_RX_SOFTIRQ."""
        self.poll_calls += 1
        work_done, called_complete = driver(self)
        return work_done, called_complete

    # -- softirq loop --------------------------------------------------------

    def run_softirq(self, driver, refill=None, max_rounds=8):
        """One NET_RX_SOFTIRQ pass. net_rx_action runs at most a bounded quota
        of poll rounds per invocation; an instance whose poll keeps returning
        the full budget stays SCHEDULED and is re-queued for the next pass.
        A driver that completes (work_done < budget) ends the pass."""
        rounds = []
        for _ in range(max_rounds):
            if self.state == "IDLE":
                break
            if refill is not None:
                self.ring = refill(self.ring, self.budget)
            work_done, called_complete = self.poll_round(driver)
            rounds.append((work_done, called_complete))
            if called_complete and self.state == "IDLE":
                break
        return rounds


# -- driver implementations --------------------------------------------------

def good_driver(napi):
    """Process up to budget packets; complete only when ring is drained."""
    budget = napi.budget
    work_done = 0
    while work_done < budget and napi.ring > 0:
        # one descriptor: build skb, hand to GRO, count it
        napi.ring -= 1
        work_done += 1
    called_complete = False
    if work_done < budget:
        # ring drained (or empty): complete and re-enable IRQ
        napi.napi_complete_done(work_done)
        napi.irq_enabled = True
        called_complete = True
    # else: budget exhausted, stay SCHEDULED, do NOT complete
    return work_done, called_complete


# -- scenario tests -----------------------------------------------------------

def scenario_heavy_load():
    """(a) Ring refilled to 200 each round; budget 64. Correct driver must
    stay SCHEDULED, never complete, keep IRQ masked."""
    napi = Napi()
    refilled = 0

    def refill(ring, budget):
        nonlocal refilled
        refilled += 1
        return 200                      # NIC keeps delivering

    napi.irq(200)                        # one IRQ fires
    rounds = napi.run_softirq(good_driver, refill=refill)

    ok = (
        napi.state == "SCHEDULED"
        and napi.irq_enabled is False
        and napi.completes == 0
        and napi.irq_arrivals == 1
        and napi.schedules == 1
        and all(wd == BUDGET and not cc for wd, cc in rounds)
    )
    print(f"[{('PASS' if ok else 'FAIL')}] heavy load: {len(rounds)} poll "
          f"rounds, all returned budget={BUDGET}, stays SCHEDULED, IRQ masked, "
          f"completes={napi.completes}, schedules={napi.schedules}/{napi.irq_arrivals}")
    return ok


def scenario_light_load():
    """(b) Ring has 5 packets, budget 64. Must complete, return 5, re-enable IRQ."""
    napi = Napi()
    napi.irq(5)
    rounds = napi.run_softirq(good_driver, refill=None)

    work_done = rounds[0][0] if rounds else None
    ok = (
        len(rounds) == 1
        and work_done == 5
        and napi.state == "IDLE"
        and napi.irq_enabled is True
        and napi.completes == 1
        and napi.ring == 0
    )
    print(f"[{'PASS' if ok else 'FAIL'}] light load: work_done={work_done}, "
          f"state={napi.state}, IRQ re-enabled={napi.irq_enabled}, "
          f"completes={napi.completes}")
    return ok


def scenario_irq_reentrancy():
    """(c) IRQ arriving while SCHEDULED must be dropped: no re-schedule, no
    re-entry into poll, no IRQ storm."""
    napi = Napi()
    napi.irq(200)
    napi.irq(180)                        # IRQ fires again while scheduled
    napi.irq(150)
    rounds = napi.run_softirq(good_driver, refill=lambda r, b: 200)

    ok = (
        napi.schedules == 1
        and napi.irq_arrivals == 3
        and napi.poll_calls == len(rounds)
    )
    print(f"[{'PASS' if ok else 'FAIL'}] IRQ re-entrancy: 3 IRQs but only "
          f"{napi.schedules} schedule, poll_calls={napi.poll_calls} (no re-entry)")
    return ok


def main():
    results = [
        scenario_heavy_load(),
        scenario_light_load(),
        scenario_irq_reentrancy(),
    ]
    print(f"\nNAPI model: {sum(results)}/3 scenarios PASS")
    return 0 if all(results) else 1


if __name__ == "__main__":
    raise SystemExit(main())
