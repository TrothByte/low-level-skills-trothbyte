# cpp — Skills

C++ adds object lifetimes and RAII on top of C's sharp edges. These skills cover object lifecycle, move semantics, and — uniquely — positive API design: descriptor types, RAII wrappers, typed errors, and builders that make misuse unrepresentable.

## Skills in this domain

| Skill | What it does | Stability | Path |
|---|---|---|---|
| `cpp-move-semantics` | Use when writing, reviewing, or debugging C++ code involving rvalue references, std::move and std::forward, move constructors and assignment, moved-from state and use-after-move bugs, copy elision and NRVO, the rule of five, or returning objects by value. | source-backed | `skills/cpp/cpp-move-semantics` |
| `cpp-object-lifecycle` | Use when writing, reviewing, or debugging C++ object lifetime issues: constructor/destructor order, virtual calls during construction, static initialization order fiasco, copy vs move semantics, use-after-move, destructor exceptions, dangling references and pointers, and basic.life violations. | source-backed | `skills/cpp/cpp-object-lifecycle` |
| `raii-descriptor-types-api-design` | Use when designing NEW C/C++/Rust APIs that wrap resources — file descriptors, sockets, buffers, handles, locks. Teaches positive design patterns: descriptor/newtype types, RAII ownership, typed errors, builders, and debug-asserted preconditions, so the type system makes misuse hard. | source-backed | `skills/cpp/raii-descriptor-types-api-design` |

## How to use

- Read the `SKILL.md` of a skill for its triggers, reasoning rules, and verification commands.
- `references/` hold the deep knowledge; `examples/good|bad` are verified compile-and-run fixtures;
  `evals/` define how the skill is tested.
- Load only the skill you need (see `skills/_meta/meta-routing`); references load on demand.

## Related

- [Back to repository root](../../README.md)
