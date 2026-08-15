"""
GOOD: device-ready gate + re-enumeration + flash-verify (host model).
The harness (1) flashes, (2) waits for the app banner as the readiness probe
(state, not time), (3) handles re-enumeration by re-opening the endpoint after
the flash, (4) verifies the image by checksum read-back before running tests.
This is the HIL loop the CI merge gate should require.

Run: python good/hil_readiness.py
"""
import hashlib
import time


class FakeDevice:
    def __init__(self, boot_time=0.2):
        self.boot_time = boot_time
        self.image = None
        self.banner_at = None

    def flash(self, image):
        # simulate: flash resets the device; endpoint re-enumerates
        self.image = image
        self.banner_at = time.time() + self.boot_time
        return True

    def read_back(self):
        return self.image

    def wait_for_banner(self, timeout):
        deadline = time.time() + timeout
        while time.time() < deadline:
            if self.banner_at is not None and time.time() >= self.banner_at:
                return True
            time.sleep(0.02)
        return False


def wait_for_reenumeration(dev):
    # the old endpoint is gone after flash; await the new one via the banner
    return dev.wait_for_banner(timeout=5.0)


def verify_image(dev, expected_sha):
    return hashlib.sha256(dev.read_back()).hexdigest() == expected_sha


def main():
    dev = FakeDevice()
    image = b"\x5A" * 64
    expected = hashlib.sha256(image).hexdigest()

    dev.flash(image)                       # endpoint disappears here
    assert wait_for_reenumeration(dev)     # state gate, not sleep
    assert verify_image(dev, expected)     # read-back before tests

    print("device ready (banner) + image verified -> tests may run")
    print("PASS: readiness gate + re-enumeration + flash-verify")


if __name__ == "__main__":
    main()
