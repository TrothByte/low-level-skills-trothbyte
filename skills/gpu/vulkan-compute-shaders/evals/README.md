# Evaluation — vulkan-compute-shaders

Skill: `skills/gpu/vulkan-compute-shaders`. Stability: `researched` — the GLSL and C
examples are TARGET-ONLY (no Vulkan SDK / glslangValidator on the authoring machine),
while the python models of binding-match and barrier-placement logic were actually
run on this host and their output is recorded below.

## Verified facts (host, recorded 2026-08-20)

Python 3.11.9, plain interpreter, no third-party packages. Real output:

```
$ python examples/tools/binding_check.py examples/good/compute.comp examples/good/host_layout.json
shader  : examples/good/compute.comp
host    : examples/good/host_layout.json
local_size_x = 64  (workgroup size; dispatch count is separate)
shader bindings:
  set=0 binding=0 STORAGE_BUFFER InBuf
  set=0 binding=1 STORAGE_BUFFER OutBuf
  set=0 binding=2 UNIFORM_BUFFER Uniforms
host bindings:
  set=0 binding=0 STORAGE_BUFFER count=1 stages=['COMPUTE']
  set=0 binding=1 STORAGE_BUFFER count=1 stages=['COMPUTE']
  set=0 binding=2 UNIFORM_BUFFER count=1 stages=['COMPUTE']
VERDICT: PASS — every shader binding has a matching host binding (set, type, count, compute stage).
exit=0
```

```
$ python examples/tools/binding_check.py examples/bad/binding_mismatch.comp examples/good/host_layout.json
MISMATCH: set=0 binding=1 (Uniforms): type mismatch — shader=UNIFORM_BUFFER, host=STORAGE_BUFFER
MISMATCH: set=0 binding=2 (OutBuf): type mismatch — shader=STORAGE_BUFFER, host=UNIFORM_BUFFER
MISMATCH: set=0 binding=3 (tex): shader declares STORAGE_IMAGE but the host layout has NO such binding
VERDICT: FAIL — 3 mismatch(es); without VK_LAYER_KHRONOS_validation the device reads garbage silently.
exit=1
```

```
$ python examples/tools/barrier_check.py examples/good/good_seq.json
  verdict: SAFE (0 hazards)
exit=0
```

```
$ python examples/tools/barrier_check.py examples/bad/bad_missing_barrier_seq.json
  FLAG: HARDWARE HAZARD RAW (write->read) on buf0: op@0(write COMPUTE SHADER_WRITE) -> op@1(read COMPUTE SHADER_READ) — NO barrier between the two commands
  verdict: UNSAFE (1 hazard(s))
exit=1
```

```
$ python examples/tools/barrier_check.py examples/bad/bad_wrong_mask_seq.json
  FLAG: ... barrier present but NOT covering: dstStage ['TRANSFER'] does not cover consumer stage COMPUTE; dstAccess ['TRANSFER_READ'] does not cover consumer access SHADER_READ
  verdict: UNSAFE (1 hazard(s))
exit=1
```

```
$ python examples/tools/barrier_check.py examples/bad/bad_wrong_stage_seq.json
  FLAG: ... barrier present but NOT covering: srcStage ['VERTEX'] does not cover producer stage COMPUTE
  verdict: UNSAFE (1 hazard(s))
exit=1
```

Verified model facts (host, no GPU needed):
- A shader binding with no host counterpart, a type mismatch (storage vs uniform vs
  image), a shader array count exceeding the host `descriptorCount`, and missing
  `COMPUTE` in `stageFlags` are all detected by `binding_check.py`.
- A write->read dependency on the same buffer with no barrier between the commands is
  flagged as RAW; read->write is flagged as WAR.
- A barrier present but with non-covering dst masks (transfer-only for a compute
  reader) or a wrong src stage (VERTEX for a compute producer) is flagged with the
  exact failing mask.

## Synthetic evals

- easy/negative: `binding_check.py` on `examples/bad/binding_mismatch.comp` must FAIL
  and name each mismatched (set, binding) with its type (recorded above).
- easy/negative: `examples/good/compute.comp` + `examples/good/host_layout.json` must
  PASS 1:1 (recorded above).
- medium/negative: `bad_missing_barrier_seq.json` — two dispatches, second reads the
  first's output, no barrier — must be flagged (recorded above).
- medium/negative: `bad_wrong_mask_seq.json` — barrier exists but dst masks cover only
  TRANSFER while the consumer is a COMPUTE read — must be flagged (recorded above).
- hard/negative: `bad_wrong_stage_seq.json` — `VK_PIPELINE_STAGE_VERTEX_SHADER_BIT`
  used as srcStage for a compute producer — must be flagged (recorded above).
- easy/negative: wrong global index math (`gid` from dispatch count alone) in a
  shader review — must be caught by code review, no tool exists.
- easy/negative: hand-written SPIR-V instead of `glslangValidator` output — must be
  rejected.

## False-positive evals (correct code must NOT be flagged)

- Correct shader/host mirror (`compute.comp` + `host_layout.json`) — PASS, no
  mismatches invented.
- Correct narrow barrier masks (`good_seq.json` covering both COMPUTE SHADER_READ and
  TRANSFER TRANSFER_READ consumers) — SAFE, must NOT be "strengthened" to
  `ALL_COMMANDS` / `MEMORY_READ`/`MEMORY_WRITE` or flagged as missing.
- Host-only extra bindings in the layout (legal in Vulkan) — reported as a `note`,
  not a mismatch.
- A barrier that correctly covers a transfer consumer but not a compute consumer must
  be flagged only for the compute edge, not both edges.
- `VK_ACCESS_MEMORY_READ_BIT` / `VK_ACCESS_MEMORY_WRITE_BIT` in a barrier — allowed by
  the model (broad access flags cover any access); the review may note them as weak
  but must not call them incorrect.

## Historical evals

- Validation-layer-only-catchable binding bugs: the `binding_mismatch` class (type
  swap + missing binding) compiles, creates a pipeline, and silently reads garbage.
  Historical record: without `VK_LAYER_KHRONOS_validation` this class goes
  unreported; the host model catches it statically.
- Barrier-free compute races: the `missing_barrier` class (same-queue write/read)
  is timing-dependent — passes golden runs on many GPUs, fails on others. Matches
  the ISO-Bench / AgentKernelArena "passes review, fails under load" pattern for
  GPU kernels.
- Mask-confusion class: wrong stage (`VERTEX` for compute) or wrong access masks
  are valid API calls that pass validation and fail correctness only on hardware.

## Adversarial evals

- A pipeline that "works" on the author's GPU by luck: two dispatches with no barrier
  (`bad_missing_barrier_seq.json`) — the agent must refuse to certify it and must
  insert `src=COMPUTE|SHADER_WRITE, dst=COMPUTE|SHADER_READ` between the dispatches.
- A shader that compiles but binds descriptors the host never declared
  (`binding_mismatch.comp`) — the agent must not accept "it compiles" as evidence;
  must run `binding_check.py` and demand validation layers.
- A "helpful" suggestion to fix everything with `VK_PIPELINE_STAGE_ALL_COMMANDS_BIT`
  + `VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT` — acceptable as a last
  resort, but the agent must prefer narrow masks and must never use a wrong specific
  mask (the `VERTEX` case) in its place.

## Verification commands (target — Vulkan SDK, validation layers, glslangValidator)

To promote to `verified`, run on a machine with the Vulkan SDK:

```
glslangValidator -V examples/good/compute.comp -o examples/good/compute.spv
glslangValidator -V examples/bad/binding_mismatch.comp -o examples/bad/binding_mismatch.spv
glslangValidator -V examples/bad/missing_barrier.comp -o examples/bad/missing_barrier.spv
```

Expected: all three compile (the bad files are valid GLSL — their bugs are
host-side and invisible to the compiler). Then build `examples/good/pipeline_setup.c`
with the Vulkan headers and link against vulkan-1, create the instance/device with
`VK_LAYER_KHRONOS_validation` enabled, and confirm:
- pipeline creation succeeds with zero validation errors;
- the descriptor-layout mirror triggers `VUID` errors when the bad
  `binding_mismatch.c` layout is used instead;
- the `vkCmdPipelineBarrier` between dispatch and `vkCmdCopyBuffer` is required:
  removing it produces a `SYNC` validation error or (without layers) stale reads.

## Scoring

- detection: names the mismatched (set, binding, type) or the missing/uncoviced
  dependency edge and the exact failing (non-covering) stage/access mask.
- reasoning: explains the mechanism (shader/host descriptor contract;
  same-queue order is not visibility; barrier masks are a covering relation).
- fix: minimal change at the right binding/barrier; does not over-strengthen
  (no blanket `ALL_COMMANDS`/`MEMORY_READ` everywhere) and never uses a wrong
  specific mask.
- verification: cites `binding_check.py` / `barrier_check.py` (host) and
  `glslangValidator -V` + validation layers (target), and states that the authoring
  machine lacks the Vulkan SDK.
