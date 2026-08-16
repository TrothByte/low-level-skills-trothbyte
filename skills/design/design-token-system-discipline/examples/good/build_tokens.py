# GOOD: minimal style-dictionary-style build — tokens.json is the single
# source of truth; one build emits primitive AND semantic CSS custom
# properties. The component layer only ever references the emitted vars.
"""Mini style-dictionary build for design-token-system-discipline.

Resolves DTCG aliases ({a.b.c}) recursively and prints a :root CSS custom
property block. Fails loudly on unresolved aliases. Run:
    python build_tokens.py ../../examples/good/tokens.json
"""
import json
import re
import sys

ALIAS_RE = re.compile(r"^\{([a-zA-Z0-9_.-]+)\}$")


def collect(tree, prefix=()):
    out = {}
    for k, v in tree.items():
        p = prefix + (k,)
        if isinstance(v, dict) and "$value" not in v:
            out.update(collect(v, p))
        else:
            out[p] = v
    return out


def resolve(tokens, value, depth=0):
    if depth > 32:
        raise ValueError("alias cycle")
    if isinstance(value, dict):
        return {k: resolve(tokens, v, depth + 1) for k, v in value.items()}
    m = ALIAS_RE.match(value) if isinstance(value, str) else None
    if m:
        target = tuple(m.group(1).split("."))
        if target not in tokens:
            raise ValueError(f"unresolved alias {{.{'.'.join(target)}}}")
        return resolve(tokens, tokens[target]["$value"], depth + 1)
    return value


def to_var(p):
    return "--" + "-".join(p).lower()


def main():
    if len(sys.argv) != 2:
        print("usage: build_tokens.py <tokens.json>")
        return 2
    data = json.load(open(sys.argv[1], encoding="utf-8"))
    tokens = collect(data)
    emitted = []
    for p, node in tokens.items():
        val = resolve(tokens, node["$value"])
        if isinstance(val, dict):
            for k, v in val.items():
                emitted.append(f"  {to_var(p) + '-' + k}: {v};")
        else:
            emitted.append(f"  {to_var(p)}: {val};")
    print(":root {")
    print("\n".join(emitted))
    print("}")
    sem = [p for p in tokens if p[0] == "semantic"]
    print(f"# {len(tokens)} tokens built from {sys.argv[1]}; "
          f"{len(sem)} semantic tokens resolved to primitives")


if __name__ == "__main__":
    main()
