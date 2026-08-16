# GOOD: a lock-order cycle detector modeling lockdep's dependency graph.
# Edges L1 -> L2 mean "L1 held while L2 acquired". A cycle (strong path
# back to a lock on the same acquisition chain) is a deadlock class. The
# detector finds the AB-BA cycle that the bad model misses.
# Run: python examples/good/lockdep_cycle_detect.py   (expect exit 0)

from collections import defaultdict

class Graph:
    def __init__(self):
        self.edges = defaultdict(set)
    def add(self, first, second):
        self.edges[first].add(second)

def find_cycle(g):
    """DFS over dependency edges; a cycle reachable from itself is AB-BA."""
    def dfs(node, path):
        if node in path:
            idx = path.index(node)
            return path[idx:] + [node]
        for nxt in g.edges.get(node, ()):
            r = dfs(nxt, path + [node])
            if r:
                return r
        return None
    for n in list(g.edges):
        r = dfs(n, [])
        if r:
            return r
    return None

def main():
    g = Graph()
    # whole kernel: three acquisition sites
    g.add("A", "B")     # site 1: takes A then B
    g.add("B", "A")     # site 2: takes B then A  -> cycle A->B->A
    g.add("C", "B")     # site 3 (third caller, must not be missed)
    cycle = find_cycle(g)
    print(f"cycle: {cycle}")
    assert cycle is not None and cycle[0] == cycle[-1]
    print("GOOD: AB-BA cycle detected from the global graph (all sites)")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
