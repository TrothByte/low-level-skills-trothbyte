"""
BAD: intentionally incorrect — no USB/DFU preflight before flashing.
Simulates the openmotion #95 class: the target is in DFU mode, the flash tool
is invoked without checking the device state, and the agent retries forever
instead of diagnosing the mode.

Run: python bad/missing_preflight.py
"""
import time


def usb_device_state():
    # the target is stuck in DFU (bootloader) mode
    return "dfu_mode"


def flash_tool():
    return False  # can't reach the target in DFU mode


def main():
    attempts = 0
    while attempts < 3:
        # BUG: no preflight of usb_device_state()
        ok = flash_tool()
        if ok:
            print("flashed")
            return
        attempts += 1
        time.sleep(0.1)
    print("can't reliably flash (retried blindly; device was in DFU)")


if __name__ == "__main__":
    main()
