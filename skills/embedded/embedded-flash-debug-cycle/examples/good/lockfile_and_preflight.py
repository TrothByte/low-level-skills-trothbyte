"""
GOOD: stale-lock reclaim + USB/DFU preflight (host-runnable model).
A lock carries owner PID; a dead owner's lock is reclaimed, and flashing is
preflighted against the device state so a DFU target is diagnosed instead of
blindly retried.

Run: python good/lockfile_and_preflight.py
"""
import os
import tempfile


def owner_alive(pid):
    try:
        os.kill(pid, 0)   # signal 0 = existence check
        return True
    except (OSError, ProcessLookupError):
        return False


def acquire_lock(lock_path, owner_pid):
    if os.path.exists(lock_path):
        with open(lock_path) as f:
            stored = int(f.read().strip())
        if not owner_alive(stored):
            os.remove(lock_path)          # stale lock: reclaim
    try:
        fd = os.open(lock_path, os.O_CREAT | os.O_EXCL | os.O_WRONLY)
        os.write(fd, str(owner_pid).encode())
        os.close(fd)
        return True
    except FileExistsError:
        return False


def usb_device_state():
    return "ready"     # preflight: probe + target enumerated, not in DFU


def flash_ok():
    return usb_device_state() == "ready"


def main():
    lock = os.path.join(tempfile.gettempdir(), "flash.lock")

    # simulate a crashed previous run leaving a stale lock
    with open(lock, "w") as f:
        f.write("999999999")      # nonexistent owner PID
    assert acquire_lock(lock, os.getpid()), "stale lock must be reclaimed"

    if usb_device_state() != "ready":
        print("target not ready (DFU?) — diagnose, do not retry")
    else:
        assert flash_ok()
        print("preflight ok; flashed")

    os.remove(lock)
    print("PASS: stale-lock reclaim + preflight")


if __name__ == "__main__":
    main()
