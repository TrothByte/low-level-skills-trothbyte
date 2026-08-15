# Research

The research phase of this repository. These documents are the source material for the
64-skill expansion; they record how AI agents fail in low-level programming, map the
surrounding skill-repository ecosystem, and define the implementation contract.

| Document | Contents |
|---|---|
| [`2026-08-15-agent-failures-survey.md`](2026-08-15-agent-failures-survey.md) | ~55 documented AI-agent failures in low-level code (CONCUR, RustEvo², crate hallucinations, Ghostty, ISO-Bench, crypto-Rust, ...) with root-cause analysis and false-positive calibration data |
| [`2026-08-15-asm-agent-failures-survey.md`](2026-08-15-asm-agent-failures-survey.md) | Assembly hallucinations: invented mnemonics/opcodes, AT&T operand inversion, Thumb-2 errors, plausible-but-wrong decompilation, LLM-as-compiler calibration |
| [`2026-08-15-external-repos-audit.md`](2026-08-15-external-repos-audit.md) | Audit of 18+ external skill repositories: confirmed Zig gap, new domains (virtualization, RISC-V/CHERI, HPC, HDL, Android RE), methodology and licensing lessons |
| [`2026-08-15-new-skills-prompt.md`](2026-08-15-new-skills-prompt.md) | The final implementation prompt for the 64 new skills (batches A–W, quality bar, acceptance criteria, anti-duplication rules) |

The consolidated machine-readable state lives in `registry/` (skills, sources, claims,
cross-links) and `roadmap/`. These surveys are analytical source material — normative
claims inside the skills trace to primary sources via `registry/claims.yaml` and
`registry/sources.yaml`, never to the surveys themselves.
