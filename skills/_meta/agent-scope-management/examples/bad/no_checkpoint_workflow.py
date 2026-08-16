# BAD: no checkpointing. All results live in the session's "memory"; a
# simulated interruption wipes them, yet the workflow still prints
# "all units complete". This is the failure mode that loses days of work.
# Marker: intentionally incorrect
# Run: python examples/bad/no_checkpoint_workflow.py

class Memory:
    """intentionally incorrect: the agent's context window as the ONLY
       store; it evaporates on interruption."""
    def __init__(self):
        self.results = {}

    def lose_all(self):
        # intentionally incorrect: interruption/compaction clears the store
        self.results = {}

def do_units(mem, count):
    for u in range(1, count + 1):
        mem.results[u] = {"unit": u}   # no file is ever written
    return count

def main():
    mem = Memory()
    n = do_units(mem, 5)

    # intentionally incorrect: the interruption is simulated AFTER the work,
    # exactly like a session whose summary never got persisted.
    mem.lose_all()

    # intentionally incorrect: "all complete" is printed from a variable
    # that survived, but the actual results are gone; a fresh session would
    # find nothing and restart from zero.
    print(f"all {n} units complete")
    print(f"resume state: results available = {len(mem.results)}")
    print("BAD: every unit's result was lost on interruption; 'complete' is "
          "claimed with no durable state and no evidence on disk")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
