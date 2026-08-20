#!/usr/bin/env python3
"""misra_topk_checker.py - heuristic scanner for MISRA C:2012 Top-k patterns.

AST-less regex + brace-tracking scanner. NOT a substitute for cppcheck
misra.py or a certified MISRA analyzer: it is a fast feedback loop for the
rule classes LLM-generated C breaks most often. Prints every violation and
exits 0 only when no violations are found.

Detected (MISRA C:2012 rule -> indicator):
  14.4  control expression not essentially Boolean (bare identifier, or bare
        bitwise/arithmetic expression, in if/while)
  12.1  precedence hidden in an unparenthesized & | ^ combined with ==/!=
  15.5  more than one return statement in a function body
  17.7  result of a non-void call (strcmp, printf, ...) discarded
  5.3   local declaration shadowing a function parameter
  17.1  string literal compared with ==/!= (constant-address comparison)

Not detectable without a type system (needs a real analyzer): enum
arithmetic, signed/unsigned essential-type mixing, dead code. See
references/misra-topk-rules.md for the full rule set.

Usage: python misra_topk_checker.py FILE.c [FILE.c ...]
Exit:  0 = no Top-k violations detected | 1 = one or more found
"""

import re
import sys

RULE_144_BARE = re.compile(r"\b(?:if|while)\s*\(\s*([A-Za-z_]\w*)\s*\)")
RULE_144_BITWISE = re.compile(r"\b(?:if|while)\s*\(\s*([^()]*[&|^][^()]*)\s*\)")
RULE_121 = re.compile(r"\b(\w+)\s*([&|^])\s*(\w+)\s*(?:==|!=)")
RULE_171 = re.compile(r'\b(\w+)\s*(?:==|!=)\s*"|"\s*(?:==|!=)\s*(\w+)')
RULE_177 = re.compile(
    r"^\s*(?:strcmp|strncmp|strcasecmp|strncasecmp|printf|fprintf|snprintf|"
    r"scanf|sscanf|fscanf|chmod|fchmod|read|write|fread|fwrite|fclose)\s*\("
)
FUNC_OPEN = re.compile(r"\b([A-Za-z_]\w*)\s*\(([^)]*)\)\s*\{")
PARAM_NAME = re.compile(r"\b([A-Za-z_]\w*)\s*(?:,|\))")
LOCAL_DECL = re.compile(
    r"\b(?:int|char|long|short|float|double|unsigned|signed|size_t|"
    r"uint8_t|uint16_t|uint32_t|uint64_t|int8_t|int16_t|int32_t|int64_t)\s+"
    r"(?:[A-Za-z_]\w*\s*\*\s*)?([A-Za-z_]\w*)"
)
KEYWORDS = {"if", "while", "for", "switch", "return", "sizeof", "else"}


def looks_boolean(expr):
    return any(op in expr for op in ("&&", "||", "==", "!=", "<=", ">=", "<", ">", "!", "?"))


def find_functions(text):
    """Yield (name, regex_match, body_start, body_text) for each function."""
    for m in FUNC_OPEN.finditer(text):
        name = m.group(1)
        if name in KEYWORDS:
            continue
        brace = text.index("{", m.start())
        depth = 0
        i = brace
        while i < len(text):
            if text[i] == "{":
                depth += 1
            elif text[i] == "}":
                depth -= 1
                if depth == 0:
                    yield name, m, brace + 1, text[brace + 1:i]
                    break
            i += 1


def scan(path):
    with open(path, encoding="utf-8") as fh:
        text = fh.read()

    findings = []
    for lineno, line in enumerate(text.splitlines(), 1):
        for m in RULE_144_BARE.finditer(line):
            findings.append((lineno, "14.4",
                "control expression is not essentially Boolean: if/while (%s)" % m.group(1)))
        for m in RULE_144_BITWISE.finditer(line):
            if not looks_boolean(m.group(1)):
                findings.append((lineno, "14.4",
                    "bitwise/arithmetic expression used as a control expression"))
        for m in RULE_121.finditer(line):
            findings.append((lineno, "12.1",
                "unparenthesized `%s %s %s` next to ==/!= relies on precedence"
                % (m.group(1), m.group(2), m.group(3))))
        for m in RULE_171.finditer(line):
            findings.append((lineno, "17.1",
                "string literal compared with ==/!= (constant-address comparison)"))
        m177 = RULE_177.match(line)
        if m177:
            findings.append((lineno, "17.7",
                "return value of `%s` discarded" % m177.group(0).split("(")[0].strip()))

    for name, m, body_start, body in find_functions(text):
        returns = len(re.findall(r"\breturn\b", body))
        if returns > 1:
            fline = text[:m.start()].count("\n") + 1
            findings.append((fline, "15.5",
                "function `%s` has %d return statements (Rule 15.5 requires a single exit)"
                % (name, returns)))
        params = m.group(2).strip()
        if params and params != "void":
            param_names = set(PARAM_NAME.findall(params))
            for dm in LOCAL_DECL.finditer(body):
                if dm.group(1) in param_names:
                    dline = text[:body_start + dm.start()].count("\n") + 1
                    findings.append((dline, "5.3",
                        "local `%s` shadows a parameter of function `%s`"
                        % (dm.group(1), name)))

    return findings


def main():
    paths = sys.argv[1:]
    if not paths:
        print("usage: misra_topk_checker.py FILE.c [FILE.c ...]")
        return 2

    total = 0
    for path in paths:
        findings = scan(path)
        if findings:
            print("%s: %d Top-k violation(s) detected:" % (path, len(findings)))
            for lineno, rule, msg in sorted(findings):
                print("  %s:%d: [%s] %s" % (path, lineno, rule, msg))
            total += len(findings)
        else:
            print("%s: OK - no Top-k violations detected" % path)

    if total:
        print("\nFAIL: %d violation(s) across %d file(s)" % (total, len(paths)))
        return 1
    print("\nPASS: no Top-k violations detected across %d file(s)" % len(paths))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
