# Agent -> Ghidra triage -> annotate -> type-recovery -> diff loop (PyGhidra).
# Target code for the machine that has Ghidra + pyghidra installed. Not run on
# this host (toolchain absent). Pipeline and data flow follow pyghidra-docs /
# ghidra-api; exact API names must be checked against the installed Ghidra
# version (program.getImageBase() vs currentProgram, etc.).
import json
import sys

import pyghidra


def analyze(path, out):
    with pyghidra.open_program(path) as flat_api:
        program = flat_api.getCurrentProgram()
        fm = program.getFunctionManager()
        base = str(program.getImageBase())
        report = {"program": path, "image_base": base, "functions": []}
        for fn in fm.getFunctions(True):
            body = fn.getBody()
            callees = [str(c.getEntryPoint()) for c in fm.getCalledFunctions(fn)]
            report["functions"].append({
                "name": fn.getName(),
                "entry": str(fn.getEntryPoint()),
                "size": body.getNumAddresses() if body else 0,
                "calls": callees,
            })
        # Triage is the first loop turn: rank functions by size, callee fan-in,
        # and cross-references. Annotation and type recovery happen in later
        # turns (createLabel / decompile / dataTypeManager) and the report is
        # re-diffed against this baseline after every change.
        with open(out, "w", encoding="utf-8") as f:
            json.dump(report, f, indent=2)
        print("base=%s functions=%d" % (base, len(report["functions"])))


if __name__ == "__main__":
    analyze(sys.argv[1], sys.argv[2])
