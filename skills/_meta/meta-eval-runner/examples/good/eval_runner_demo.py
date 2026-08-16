# GOOD: eval_runner_demo.py
"""
Minimal synthetic eval runner for the
meta-eval-runner skill. Demonstrates the full eval loop:

  fixtures -> gate -> verdict -> confusion matrix -> metrics

The "agent under test" has a real defect: its clamp clamps to [0,99] instead
of [0,100]. The eval's job is to expose that defect: fixtures where the
defect manifests must FAIL (kind="negative"), fixtures where the code is
correct must PASS (kind="positive"), and the boundary case is marked
"ambiguous" (needs an explicit resolution).

The metrics (precision / recall / FP-rate) are derived from the recorded
confusion matrix, never estimated.

Run: python examples/good/eval_runner_demo.py
"""


def agent_clamp_impl(x):
    """The 'agent under test'. Has a defect: clamps to [0,99], not [0,100]."""
    return max(0, min(99, x))


def ground_truth_clamp(x):
    """The oracle the gate compares against."""
    return max(0, min(100, x))


# Fixtures: (name, input, kind)
#   positive  -> the agent is CORRECT on this input; the eval must PASS
#   negative  -> the defect manifests here;   the eval must FAIL
#   ambiguous -> boundary; the eval must resolve it explicitly
FIXTURES = [
    ("positive: in-range passthrough", 42, "positive"),
    ("positive: lower boundary", 0, "positive"),
    ("positive: below range", -3, "positive"),
    ("negative: above range", 150, "negative"),   # agent returns 99, not 100
    ("negative: upper boundary", 100, "negative"),  # agent returns 99, not 100
    ("ambiguous: exactly 100", 100, "ambiguous"),  # resolved by the oracle
]

# Expected verdict per kind: what a healthy eval must report.
EXPECTED = {"positive": "pass", "negative": "fail", "ambiguous": "fail"}


def gate(agent_out, oracle_out):
    """The executable comparator: PASS iff outputs match."""
    return "pass" if agent_out == oracle_out else "fail"


def main():
    TP = FP = TN = FN = 0
    print("== meta-eval-runner: synthetic eval demo ==")
    for name, inp, kind in FIXTURES:
        verdict = gate(agent_clamp_impl(inp), ground_truth_clamp(inp))
        expected = EXPECTED[kind]
        ok = verdict == expected

        # confusion counting: is the eval verdict right about this fixture?
        if verdict == "fail" and expected == "fail":
            TP += 1          # defect correctly flagged
        elif verdict == "pass" and expected == "pass":
            TN += 1          # correct code correctly passed
        elif verdict == "fail" and expected == "pass":
            FP += 1          # false positive: flagged correct code
        else:
            FN += 1          # false negative: missed a defect

        mark = "OK  " if ok else "MISS"
        print(f"  [{mark}] {name}: agent={agent_clamp_impl(inp)} "
              f"oracle={ground_truth_clamp(inp)} verdict={verdict} "
              f"(expected {expected})")

    print(f"\n  confusion matrix: TP={TP} FP={FP} TN={TN} FN={FN}")
    precision = TP / (TP + FP) if (TP + FP) else 1.0
    recall = TP / (TP + FN) if (TP + FN) else 1.0
    fp_rate = FP / (FP + TN) if (FP + TN) else 0.0
    print(f"  precision={precision:.2f} recall={recall:.2f} FP-rate={fp_rate:.2f}")

    # The eval MUST expose the clamp defect on every manifesting fixture.
    assert TP == 3 and FN == 0 and FP == 0, "eval must catch the clamp defect"
    print("  result: defect exposed on all negative/ambiguous fixtures, "
          "correct code never flagged")


if __name__ == "__main__":
    main()
