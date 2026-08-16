# BAD: # intentionally incorrect — parses the SMBIOS 2.x (`_SM_`) entry
# point but reads the 64-bit address field as if it were the SMBIOS 3.0
# format. The anchor selects the width; mixing them truncates or shifts
# the table base. This file MUST fail (exit != 0) when run honestly.
#
# Run: python smbios_wrong_ep.py   (expects non-zero exit)

import struct
import sys

def make_entry_point_2x():
    # 2.x entry point: _SM_ + 32-bit table address at offset 0x18
    ep = bytearray(b"_SM_" + b"\x00" * 27)
    ep[24:28] = struct.pack("<I", 0x1000)  # 32-bit address
    return bytes(ep)

def main():
    ep = make_entry_point_2x()
    # bug: assume SMBIOS 3.0 layout regardless of anchor
    # // intentionally incorrect
    table = struct.unpack("<Q", ep[20:28])[0]  # reads 2.x padding/next field
    if table == 0x1000:
        print("PASS: correct")
        return 0
    print("BUG: wrong table base %#x (expected 0x1000)" % table)
    return 1  # the correct failure: width selection is wrong

if __name__ == "__main__":
    sys.exit(main())
