# BAD: # intentionally incorrect — walks a DTB whose magic is corrupted
# without rejecting it. A DTB with bad magic is garbage; parsing further
# yields invented node names / properties. Validating the magic is the
# first gate. This file MUST fail (exit != 0) when run honestly.
#
# Run: python fdt_bad_magic.py   (expects non-zero exit)

import struct
import sys

FDT_MAGIC = 0xD00DFEED
FDT_BEGIN_NODE = 0x1
FDT_END = 0x9

def corrupt_magic_dtb():
    # minimal header (40 bytes), magic byte flipped
    hdr = struct.pack(">IIIIIIIIII", FDT_MAGIC, 40, 40, 40, 40, 17, 17, 0, 0, 0)
    b = bytearray(hdr)
    b[3] ^= 0x01  # flip a magic byte: // intentionally incorrect
    return bytes(b)

def main():
    dtb = corrupt_magic_dtb()
    magic = struct.unpack(">I", dtb[:4])[0]
    # bug: accept any magic, only check length  # intentionally incorrect
    if len(dtb) >= 40:
        print("BUG: accepted DTB with magic %08x" % magic)
        return 0  # silently accepted — the failure
    return 1

if __name__ == "__main__":
    sys.exit(main())
