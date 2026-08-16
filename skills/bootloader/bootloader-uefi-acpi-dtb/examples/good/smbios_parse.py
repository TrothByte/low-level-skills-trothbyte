# GOOD: parse the SMBIOS entry point, dispatching on the anchor to pick
# the correct table-address width. SMBIOS 3.0 uses `_SM3_` with a 64-bit
# table address; the 2.x entry point uses `_SM_` with a 32-bit address.
# Wrong width selection truncates the table base and walks garbage.
#
# Run: python smbios_parse.py   (expects PASS, exit 0)

import struct
import sys

def make_entry_point(version3):
    if version3:
        # SMBIOS 3.0 entry point (28 bytes): _SM3_ anchor, entry point
        # size at 16, 64-bit table address at offset 0x14 (20).
        ep = bytearray(28)
        ep[0:5] = b"_SM3_"
        ep[5] = 0x5f  # checksum placeholder (not validated here)
        ep[6] = 24                       # entry point length
        ep[7] = 0x03                     # major
        ep[8] = 0x00                     # minor
        ep[16:20] = struct.pack("<I", 24)  # entry point size
        ep[20:28] = struct.pack("<Q", 0x100000)  # 64-bit table address
        return bytes(ep)
    else:
        # 2.x entry point (31 bytes): _SM_ anchor, 32-bit table address
        # at offset 0x18 (24).
        ep = bytearray(31)
        ep[0:4] = b"_SM_"
        ep[4] = 0x5f
        ep[5] = 31                      # entry point length
        ep[6] = 0x02                    # major
        ep[7] = 0x00                    # minor
        ep[24:28] = struct.pack("<I", 0x1000)  # 32-bit table address
        return bytes(ep)

def parse_ep(ep):
    if ep[:5] == b"_SM3_":       # anchor is 5 bytes
        return struct.unpack("<Q", ep[20:28])[0]  # 64-bit at offset 20
    if ep[:4] == b"_SM_":        # anchor is 4 bytes
        return struct.unpack("<I", ep[24:28])[0]  # 32-bit at offset 24
    return None

def main():
    ep3 = make_entry_point(True)
    ep2 = make_entry_point(False)
    a3 = parse_ep(ep3)
    a2 = parse_ep(ep2)
    assert a3 == 0x100000, "SMBIOS 3.0 64-bit address wrong: %#x" % a3
    assert a2 == 0x1000, "SMBIOS 2.x 32-bit address wrong: %#x" % a2
    print("PASS: entry-point anchor selects the table address width")
    return 0

if __name__ == "__main__":
    sys.exit(main())
