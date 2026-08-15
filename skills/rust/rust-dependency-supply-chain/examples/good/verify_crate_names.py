"""Proxy check: does each candidate crate name exist on crates.io?

PROXY NOTICE: this script compares candidate names against a hardcoded list of
well-known real crate names using Levenshtein distance. It is an offline
stand-in for the crates.io API (GET https://crates.io/api/v1/crates/<name>)
and for `cargo info <name>` when the network is unavailable. In production,
always confirm with the API or `cargo info` (see evals/README.md).
Exact match => real. Distance 1..2 => near-miss / typosquat risk, do not use.
Distance > 2 or unknown => verify before adding.
"""

REAL = [
    "serde", "serde_json", "tokio", "rand", "clap", "anyhow", "thiserror",
    "reqwest", "hyper", "tracing", "bytes", "futures", "log", "num",
    "smallvec", "itertools", "rayon", "regex", "once_cell", "aes-gcm",
    "chacha20poly1305", "poly1305", "ring", "zeroize", "hex", "sha2", "url",
    "chrono", "uuid", "syn", "quote", "proc-macro2",
]

CANDIDATES = [
    "serde",                 # real
    "tokio",                 # real
    "chacha20poly1305",      # real
    "serde_jon",             # hallucination (typo of serde_json)
    "serde-json",            # near-miss of serde_json
    "tokio-utils-rs",        # near-miss of the real tokio-utils
    "chacha20poly",          # near-miss of chacha20poly1305
    "cha-cha20-poly1305",    # near-miss of chacha20poly1305
    "rust-crypto-aes-gcm",   # generic name mimicking the real aes-gcm
    "clap-rs2",              # near-miss of clap
    "rand-chacha-ng",        # near-miss of rand (rand_chacha is the real crate)
    "serde_jason",           # typo of serde_json
    "totally-made-up-crate", # unknown, verify
]


def levenshtein(a: str, b: str) -> int:
    if a == b:
        return 0
    if not a:
        return len(b)
    if not b:
        return len(a)
    prev = list(range(len(b) + 1))
    for i, ca in enumerate(a, 1):
        cur = [i]
        for j, cb in enumerate(b, 1):
            cur.append(min(prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + (ca != cb)))
        prev = cur
    return prev[-1]


def classify(name: str) -> tuple[str, int]:
    if name in REAL:
        return "REAL (exact match in list)", 0
    d, closest = min((levenshtein(name, r), r) for r in REAL)
    if d <= 2:
        return f"NEAR-MISS of '{closest}' (distance {d}) - do not add unverified", d
    return f"NOT in list, closest is '{closest}' (distance {d}) - verify via crates.io API", d


def main() -> None:
    print(f"{'candidate':<24} {'verdict':<70} d")
    for c in CANDIDATES:
        verdict, d = classify(c)
        print(f"{c:<24} {verdict:<70} {d}")
    bad = [c for c in CANDIDATES if classify(c)[1] in (1, 2) or classify(c)[0].startswith("NOT")]
    print(f"\nflagged for verification: {len(bad)}/{len(CANDIDATES)} candidates")


if __name__ == "__main__":
    main()
