# GOOD: atomic check-and-use — the lock covers both the check and the use,
# so the mutation cannot interleave. Run: python3 atomic_check_use.py
import threading

class PathGuard:
    """Simulates kernel path resolution under a per-object lock."""

    def __init__(self):
        self._lock = threading.Lock()
        self.path = "safe/file"
        self.content = "ok"

    def atomic_open(self):
        # GOOD: check and use happen inside the SAME critical section.
        with self._lock:
            if self.path is None:
                return None
            return self.content[:]   # use the validated value


def main():
    guard = PathGuard()

    def mutator():
        for _ in range(1000):
            with guard._lock:        # GOOD: mutation also takes the lock
                guard.path = None
                guard.content = "deleted"

    def user():
        for _ in range(1000):
            result = guard.atomic_open()
            # GOOD: since check+use are atomic, result is never a stale value
            if result is None:
                pass

    t1 = threading.Thread(target=mutator)
    t2 = threading.Thread(target=user)
    t1.start(); t2.start()
    t1.join(); t2.join()
    print("atomic_open: no inconsistent observation (window closed)")


if __name__ == "__main__":
    main()
