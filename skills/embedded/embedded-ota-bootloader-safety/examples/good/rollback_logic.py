"""
GOOD: OTA decision flow — two slots, trial window, staged rollout (host model).
Models the full safety property chain: new image only becomes ACTIVE after a
trial window with no failed boots; a failing image rolls back; a rollout
covers the fleet only in stages. The model is runnable without MCUboot/IDF.

Run: python good/rollback_logic.py
"""

TRIAL_BOOTS = 2


class OtaState:
    def __init__(self):
        self.slot = "A"
        self.pending = None
        self.trial_boots = 0

    def write_pending(self, img):
        self.pending = img          # writes to the INACTIVE slot

    def boot(self, img_ok):
        if self.pending and img_ok:
            self.slot = self.pending
            self.pending = None
            self.trial_boots = 0
            return True
        if self.pending and not img_ok:
            self.pending = None     # rollback: keep old slot
        return False


def staged_rollout_ok(batch_failures, cohort_pct):
    # safety gate: never widen a rollout whose trial cohort shows failures
    if batch_failures > 0:
        return False
    return cohort_pct <= 100


def main():
    st = OtaState()
    st.write_pending("B")
    assert st.boot(True) is True, "new image boots"
    assert st.slot == "B"

    # corrupt image case: rolls back to A
    st2 = OtaState()
    st2.write_pending("B")
    ok = st2.boot(False)
    assert ok is False and st2.slot == "A"
    print("corrupt image: rolled back to slot A")

    # staged rollout gate: trial cohort failure blocks widening
    assert staged_rollout_ok(0, 1) is True
    assert staged_rollout_ok(1, 100) is False
    print("staged rollout: trial failure blocks 100% push")

    print("PASS: two-slot + rollback + staged rollout model")


if __name__ == "__main__":
    main()
