# intentionally incorrect — confident-but-wrong type recovery.
# Recovers a "type" from a single instruction-width observation and asserts it
# as a fact, skipping the cross-checks (other uses, DWARF, function contract)
# that type recovery requires. The type is reported with high confidence and
# without an INFERRED/VERIFIED marker — the exact failure the
# confident-but-wrong verdict pattern produces.
def recover_types(flat_api, fn):
    listing = flat_api.getCurrentProgram().getListing()
    types = {}
    for insn in listing.getInstructions(fn.getBody(), True):
        mnem = insn.getMnemonicString()
        if mnem.startswith("movzbl") and "param0" not in types:
            types["param0"] = "char"     # one observation, asserted as fact
        if mnem.startswith("movsd") and "return" not in types:
            types["return"] = "string"   # movsd is a double load, not a string
    print("RECOVERED (high confidence):", types)
    return types
