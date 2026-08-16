# BAD: classic check-then-use — the check validates a pathname, the use
# opens the CURRENT pathname. The mutator swaps the pathname between them, so
# the victim opens the attacker's path despite the check passing.
# This run FORCES the interleaving with events so the window is observable.
# intentionally incorrect
import threading

class PathResource:
    def __init__(self):
        self._lock = threading.Lock()
        self.path = "safe/file"
        self.checked_ok = False

    def check(self, name):
        with self._lock:                    # check: validate the CURRENT name
            return name == "safe/file"

    def use(self, name):
        with self._lock:                    # use: act on the CURRENT name
            # BAD: the name was validated earlier, but we re-read the live
            # value here — an attacker can swap `path` in between.
            return f"opened {self.path} (current value)"


def main():
    res = PathResource()
    checked = threading.Event()
    swapped = threading.Event()
    results = []

    def attacker():
        checked.wait()                      # wait for the check to pass...
        with res._lock:
            res.path = "attacker/replaced"  # ...then swap the path
        swapped.set()

    def victim():
        if res.check("safe/file"):          # T1: check passes on "safe/file"
            checked.set()
            swapped.wait()                  # T2 window: attacker swaps
            results.append(res.use("safe/file"))  # T3: use re-reads live path

    t1 = threading.Thread(target=attacker)
    t2 = threading.Thread(target=victim)
    t1.start(); t2.start()
    t1.join(); t2.join()

    if results and "attacker/replaced" in results[0]:
        print(f"TOCTOU OBSERVED: check passed on 'safe/file' but use opened "
              f"{results[0]!r}")
    else:
        print("window not observed this run")


if __name__ == "__main__":
    main()
