# intentionally incorrect
# Generates a Cargo.toml from candidate names WITHOUT any existence check.
# Three of the five names below do not exist on crates.io (serde_jon,
# chacha20poly, tokio-utils-rs). Shipping this breaks the build at `cargo
# add` / `cargo fetch` time and would invite a typosquat if a squat appeared.

CANDIDATES = ["serde", "serde_jon", "tokio", "chacha20poly", "tokio-utils-rs"]


def emit_toml(deps):
    lines = ["[package]", 'name = "generated"', 'version = "0.1.0"',
             'edition = "2021"', "", "[dependencies]"]
    for name in deps:
        lines.append(f'{name} = "1"')
    return "\n".join(lines)


def main():
    print(emit_toml(CANDIDATES))


if __name__ == "__main__":
    main()
