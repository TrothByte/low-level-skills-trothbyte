# GOOD: walk a Flattened Device Tree (DTB) strictly by header offsets.
# Validates magic 0xd00dfeed, uses off_dt_struct / off_dt_strings, handles
# 4-byte property-string and 8-byte node/string-block alignment. This is
# the host-runnable core of the DTB handoff logic in a bootloader.
#
# Run: python fdt_walk.py   (expects PASS, exit 0)

import struct
import sys

FDT_MAGIC = 0xD00DFEED
FDT_BEGIN_NODE = 0x1
FDT_END_NODE = 0x2
FDT_PROP = 0x3
FDT_NOP = 0x4
FDT_END = 0x9

def align4(x):
    return (x + 3) & ~3

def align8(x):
    return (x + 7) & ~7

def build_fdt():
    """Build a tiny synthetic DTB: /soc@100 { compatible = "acme,chip"; }."""
    struct_block = bytearray()
    strings_block = bytearray()
    strtab = {}

    def add_string(s):
        nonlocal strings_block, strtab
        if s not in strtab:
            strtab[s] = len(strings_block)
            strings_block += s.encode() + b"\x00"
        return strtab[s]

    def prop(name, value):
        nonlocal struct_block
        off = add_string(name)
        struct_block += struct.pack(">I", FDT_PROP)
        struct_block += struct.pack(">II", len(value), off)
        struct_block += value
        while len(struct_block) % 4:
            struct_block += b"\x00"

    def begin_node(name):
        nonlocal struct_block
        struct_block += struct.pack(">I", FDT_BEGIN_NODE)
        struct_block += name.encode() + b"\x00"
        while len(struct_block) % 4:
            struct_block += b"\x00"

    def end_node():
        nonlocal struct_block
        struct_block += struct.pack(">I", FDT_END_NODE)

    begin_node("")
    begin_node("soc@100")
    prop("compatible", b"acme,chip")
    prop("reg", struct.pack(">II", 0x100, 0x1000))
    end_node()
    end_node()
    struct_block += struct.pack(">I", FDT_END)

    # reserve map: one empty entry (8-byte aligned structure base)
    rsvmap = struct.pack(">QQ", 0, 0)
    # header (40 bytes) + rsvmap + struct + strings, align struct block to 8
    hdr_size = 40
    off_rsv = hdr_size
    off_struct = align8(off_rsv + len(rsvmap))
    off_strings = align4(off_struct + len(struct_block))
    total = align8(off_strings + len(strings_block))

    hdr = struct.pack(
        ">IIIIIIIIII", FDT_MAGIC, total, off_struct, off_strings,
        off_rsv, 17, 17, 0, len(strings_block), len(struct_block))
    return hdr + rsvmap + b"\x00" * (off_struct - hdr_size - len(rsvmap)) + \
        bytes(struct_block) + b"\x00" * (off_strings - off_struct - len(struct_block)) + \
        bytes(strings_block) + b"\x00" * (total - off_strings - len(strings_block))

def walk_fdt(dtb):
    if len(dtb) < 40:
        return False
    magic, total, off_struct, off_strings, off_rsv, ver, lcver, bcpuid, szdt, szdts = \
        struct.unpack(">IIIIIIIIII", dtb[:40])
    if magic != FDT_MAGIC:
        return False
    if ver < 16:
        return False
    # walk structure block
    p = off_struct
    found = {}
    while p < off_strings:
        tok = struct.unpack(">I", dtb[p:p + 4])[0]
        p += 4
        if tok == FDT_BEGIN_NODE:
            end = dtb.index(b"\x00", p)
            name = dtb[p:end].decode()
            p = align4(end + 1)
            found.setdefault(name, {})
        elif tok == FDT_END_NODE:
            pass
        elif tok == FDT_PROP:
            plen, nameoff = struct.unpack(">II", dtb[p:p + 8])
            p += 8
            value = dtb[p:p + plen]
            p = align4(p + plen)
            # name is in the strings block
            soff = off_strings + nameoff
            send = dtb.index(b"\x00", soff)
            key = dtb[soff:send].decode()
            for node in found.values():
                node[key] = value
        elif tok == FDT_NOP:
            pass
        elif tok == FDT_END:
            break
        else:
            return False
    return found

def main():
    dtb = build_fdt()
    nodes = walk_fdt(dtb)
    assert nodes, "walker returned nothing"
    assert "soc@100" in nodes, "node soc@100 missing"
    assert nodes["soc@100"]["compatible"] == b"acme,chip", "compatible mismatch"
    assert nodes["soc@100"]["reg"] == struct.pack(">II", 0x100, 0x1000), "reg mismatch"
    print("PASS: DTB magic + header + structure walk correct")
    return 0

if __name__ == "__main__":
    sys.exit(main())
