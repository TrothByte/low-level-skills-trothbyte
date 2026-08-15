"""
BAD: intentionally incorrect — test-start race (missing readiness gate).
The harness flashes and starts tests after a fixed sleep, racing the target's
boot and USB re-enumeration. On a slow cold boot the first test hits a dead
device — the openmotion #164 class. The model reproduces a device that needs
~2.5s to be ready while the harness waits only 1s.

Run: python bad/missing_readiness.py
"""
import time


class Device:
    def __init__(self, boot_time):
        self.boot_time = boot_time
        self.booted_at = None

    def flash(self):
        self.booted_at = time.time() + self.boot_time
        return True

    def is_ready(self):
        return self.booted_at is not None and time.time() >= self.booted_at


def run_tests(dev):
    # BUG: fixed sleep instead of probing device state
    time.sleep(1.0)
    if not dev.is_ready():
        return "TEST FAILED: device not ready (race)"
    return "tests passed"


def main():
    dev = Device(boot_time=2.5)   # cold boot is slower than the sleep
    dev.flash()
    print(run_tests(dev))


if __name__ == "__main__":
    main()
