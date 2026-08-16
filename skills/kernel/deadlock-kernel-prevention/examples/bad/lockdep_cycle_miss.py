# BAD: a "lock order" model that only checks the two sites named in a
# splat and misses the cycle from a third site. The reported pair is
# "fixed", the graph still contains A->B and B->A, and the model prints
# "safe".
# Marker: intentionally incorrect
# Run: python examples/bad/lockdep_cycle_miss.py

def check_pair(first, second, seen):
    # intentionally incorrect: only records the pair, never searches the
    # global dependency graph for a cycle.
    seen.add((first, second))

def main():
    seen = set()
    # intentionally incorrect: the "fix" checks only the reported pair and
    # misses that site 2 still acquires B before A.
    check_pair("A", "B", seen)
    check_pair("B", "A", seen)
    check_pair("C", "B", seen)   # third site ignored entirely

    # intentionally incorrect: verdict asserted without searching for a
    # cycle across all sites.
    if ("A", "B") in seen:
        print("SAFE: reported pair consistent (no cycle search performed)")
    print("BAD: graph still contains A->B and B->A plus C->B; the third "
          "site was never considered, so the deadlock class is missed")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
