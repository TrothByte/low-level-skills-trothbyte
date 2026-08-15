# Rebase discipline + byte patch with read-back verification (PyGhidra).
# Target code for a Ghidra + pyghidra machine. Not run on this host.
# Rules demonstrated:
#   1. Set/fix the image base BEFORE any address arithmetic. Ghidra program
#      addresses are absolute (image base + offset). The Quesma case shows the
#      failure: patching at the raw file offset $0000 instead of the loaded
#      address $A000 writes to the wrong place.
#   2. Every byte patch is verified by read-back before it is reported.
import sys

import pyghidra


def rebase_and_patch(program, offset, value):
    mem = program.getMemory()
    base = program.getImageBase()
    abs_addr = base.add(offset)      # offset is relative to the image base
    mem.setByte(abs_addr, value)
    got = mem.getByte(abs_addr)
    if got != value:
        raise RuntimeError("patch not applied: %s -> %s" % (abs_addr, got))
    print("patched %s (base %s + 0x%x) = 0x%02x, read-back 0x%02x"
          % (abs_addr, base, offset, value, got))
    return got


def main():
    path, offset_s, value_s = sys.argv[1], sys.argv[2], sys.argv[3]
    offset = int(offset_s, 0)
    value = int(value_s, 0)
    with pyghidra.open_program(path) as flat_api:
        program = flat_api.getCurrentProgram()
        rebase_and_patch(program, offset, value)


if __name__ == "__main__":
    main()
