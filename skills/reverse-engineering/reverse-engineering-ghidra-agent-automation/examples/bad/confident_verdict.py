# intentionally incorrect — confident program identity + wrong patch offset.
# Mirrors the Quesma Ghidra-MCP case (2026): Claude asserted a 6502 ROM was
# "Centipede" when it was River Raid, could not rebase ($0000 vs $A000), and
# could not write the byte patch (DEY -> NOP) — the human did it.
# This script repeats all three failure classes:
#   1. identity asserted from a 4-byte signature with fabricated confidence,
#   2. patch offset $0000 instead of the loaded address $A000,
#   3. no read-back verification of the patch.
import pyghidra


def identify(flat_api):
    program = flat_api.getCurrentProgram()
    mem = program.getMemory()
    base = program.getMinAddress()
    sig = bytes(mem.getBytes(base, 4))
    if sig == b"\x4c\x00\x00\xa0":      # jmp $A000 — matches many 6502 ROMs
        return "Centipede (confidence 0.95)"   # WRONG: it is River Raid
    return "unknown"


def patch_dey_to_nop(flat_api):
    program = flat_api.getCurrentProgram()
    addr = program.getMinAddress()       # $0000 — NOT the loaded $A000 region
    program.getMemory().setByte(addr, 0xEA)  # writes to the wrong location
    # no read-back; the patch is claimed without verification


def main():
    import sys
    with pyghidra.open_program(sys.argv[1]) as flat_api:
        verdict = identify(flat_api)
        print("IDENTITY: %s" % verdict)
        patch_dey_to_nop(flat_api)
        print("PATCH: applied (unverified)")


if __name__ == "__main__":
    main()
