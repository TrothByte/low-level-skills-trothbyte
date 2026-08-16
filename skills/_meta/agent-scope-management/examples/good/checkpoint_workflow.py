# GOOD: the checkpoint discipline. Each unit writes its result to a durable
# state file immediately; a simulated interruption loses at most the current
# unit, and a fresh "session" resumes exactly where the state file says.
# Run: python examples/good/checkpoint_workflow.py   (expect exit 0)
import json
import os
import tempfile

STATE = os.path.join(tempfile.gettempdir(), "agent_state_checkpoint.json")

def save_state(session, unit, notes, evidence):
    with open(STATE, "w", encoding="utf-8") as f:
        json.dump({"session": session, "current_unit": unit,
                   "next_action": f"unit {unit + 1}",
                   "notes": notes, "evidence": evidence}, f, indent=1)

def do_unit(session, unit):
    # simulated work that produces a result and its verification record
    result = {"unit": unit, "verification": f"exit 0 (recorded run {unit})"}
    evidence = save_state(session, unit, ["note for later"], result)
    return result

def main():
    session = "S1"
    results = []
    for u in range(1, 6):
        r = do_unit(session, u)      # checkpoint AFTER EVERY unit
        results.append(r)
        # simulate interruption right after unit 2:
        if u == 2:
            print("INTERRUPT simulated after unit 2; state file already updated")
            break

    # fresh session resumes from the durable state, not from memory
    with open(STATE, encoding="utf-8") as f:
        state = json.load(f)
    print(f"RESUME: {state['session']} continues at {state['next_action']} "
          f"(units persisted: {len(results)})")
    assert state["current_unit"] == 2 and state["next_action"] == "unit 3"
    os.unlink(STATE)
    print("GOOD: checkpoint after each unit; resume point exact, evidence recorded")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
