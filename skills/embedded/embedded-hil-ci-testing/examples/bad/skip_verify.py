"""
BAD: intentionally incorrect — no flash-verify gate.
The flash tool returns success but the image never lands (or lands corrupt);
tests run against unknown bytes and the CI "passes" on stale code. A HIL run
must read back and compare the image before testing.

Run: python bad/skip_verify.py
"""
import hashlib


def flash_device(image):
    # BUG: tool reports success; write is silently dropped
    return True, None


def verify_image(device_checksum, expected):
    return device_checksum == expected


def main():
    image = b"\x5A" * 64
    expected = hashlib.sha256(image).hexdigest()

    ok, checksum = flash_device(image)
    if ok:
        print("flash reported success")
        # BUG: no read-back; we never compare `checksum` to `expected`
        print("running tests on whatever is on the device")
    else:
        print("flash failed")


if __name__ == "__main__":
    main()
