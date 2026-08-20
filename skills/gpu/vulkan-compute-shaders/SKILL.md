---
name: vulkan-compute-shaders
description: Use when writing or reviewing Vulkan compute pipelines — SPIR-V, shader modules, descriptor sets and layouts, push constants, workgroup sizing, and memory barriers. Teaches GPU compute beyond CUDA/PTX, with binding and synchronization rules LLMs get wrong.
---

# Vulkan Compute Shaders — SPIR-V, Descriptors, Dispatch, Barriers

## When to use

- Writing or reviewing a Vulkan compute pipeline: GLSL/HLSL compiled to SPIR-V, shader
  modules, `VkComputePipeline`, descriptor sets and layouts, push constants, dispatch.
- Placing `vkCmdPipelineBarrier` calls between compute dispatches, or between compute
  and transfer (e.g. `vkCmdCopyBuffer`), so later commands observe earlier writes.
- Matching a compute shader's `layout(set=, binding=)` declarations against the host
  `VkDescriptorSetLayout` and `VkWriteDescriptorSet` data.
- Debugging garbage reads, stale data, or data races in compute workloads.
- Implementing portable GPU compute without CUDA/PTX (runs on any Vulkan device).

## When not to use

- NVIDIA-only CUDA C++ / PTX review — use `ptx-assembly`.
- Verifying kernel numerics or grid/block edge cases — use `gpu-kernel-verification-beyond-oracle`.
- Cross-thread memory-model reasoning inside a CUDA kernel — use `gpu-memory-model-coherence`.
- Tuning kernel speed or benchmarking — use `performance-measurement-discipline`.
- Detecting reward hacking in RL training kernels — use `gpu-kernel-reward-hacking-detection`.
- Graphics (vertex/fragment) pipeline review; this skill covers the compute pipeline only.

## What the agent often gets wrong

- "As long as I bind something, it works." NO. Shader `layout(set=0, binding=1)` must
  match the host `VkDescriptorSetLayoutBinding` 1:1 (set index, binding index,
  descriptor type, count, stage flags). A mismatch is the #1 silent failure: without
  validation layers the device reads garbage and no error is reported.
- "local_size and dispatch count are the same thing." NO. The shader's
  `layout(local_size_x=N)` is the workgroup size; `vkCmdDispatch(x,y,z)` is the number
  of workgroups. Total threads = `local_size * dispatch_count` per axis.
- "Same queue means the next command sees my write." NO. Writes are not implicitly
  visible to later commands on the same queue; every write→read dependency needs a
  `vkCmdPipelineBarrier` with covering stage and access masks.
- "`VK_PIPELINE_STAGE_ALL_COMMANDS_BIT` / `VK_ACCESS_MEMORY_READ_BIT` everywhere is
  fine." Valid but weak and coarse. Wrong masks (e.g. `VK_PIPELINE_STAGE_VERTEX_SHADER_BIT`
  for a compute producer) silently break correctness while still passing validation.
- "I can write SPIR-V by hand." NO. Hand-written SPIR-V is error-prone; compile
  GLSL/HLSL with `glslangValidator`/`shaderc` and inspect the generated `.spv`.
- "Push constants are free-form." NO. Every range the shader's `push_constant` block
  touches must be declared in `VkPipelineLayout` as (offset, size) with offsets/sizes
  multiples of 4; 128 bytes is the minimum guaranteed size.
- "Buffer ranges are optional." NO. `VkDescriptorBufferInfo{offset, range}` must cover
  exactly the region the shader accesses; a wrong range breaks data layout and can
  cause out-of-bounds reads inside the shader.
- "It compiles, so the pipeline works." NO. Compilation is not a binding match and not
  synchronization. The pipeline can be created fine yet read stale data; enable
  `VK_LAYER_KHRONOS_validation` and pick the compute-capable queue family.

## How to reason correctly

1. Trace the data flow first: which buffers are written by which pipeline stage and
   read by which later stage or transfer command. Then place the minimal correct
   barriers between producer and consumer.
2. Mirror the shader's binding layout exactly in `VkDescriptorSetLayout`. Write the
   table: shader `layout(set=0, binding=1)` ↔ host binding 1 =
   `VK_DESCRIPTOR_TYPE_STORAGE_BUFFER`, `VK_SHADER_STAGE_COMPUTE_BIT`, count 1.
3. Validate always: create instance and device with `VK_LAYER_KHRONOS_validation`
   enabled; treat every validation message as a release blocker.
4. Compute the global index: `gid = gl_WorkGroupID * gl_WorkGroupSize +
   gl_LocalInvocationID`; bounds-check `gid` against the total element count before
   any buffer access.
5. For every buffer write→read edge (compute→compute, compute→transfer) insert
   `vkCmdPipelineBarrier` whose src masks cover the producer and dst masks cover the
   consumer; choose the narrowest correct masks, not `ALL_COMMANDS`.
6. Compile GLSL to SPIR-V with `glslangValidator -V`; never hand-write SPIR-V.
   Prefer specialization constants for `local_size` and element counts so one shader
   serves multiple sizes.

## What to verify

- Shader binding (set, binding, type, count) matches the host descriptor layout 1:1.
- Workgroup math: `local_size * dispatch_count` covers `n`; global index is
  bounds-checked in the shader.
- A barrier exists between every write→read dependency (compute→compute,
  compute→transfer) with covering stage and access masks.
- SPIR-V is compiler-generated (`glslangValidator -V`), not hand-written.
- Pipeline creation and dispatch succeed with validation layers on, with zero
  validation errors or warnings.

## How to verify

Host (plain python 3.11, run in this repo):

```
python examples/tools/binding_check.py examples/good/compute.comp examples/good/host_layout.json
# expected: PASS — shader and host bindings match 1:1
python examples/tools/binding_check.py examples/bad/binding_mismatch.comp examples/good/host_layout.json
# expected: FAIL — type mismatches + missing host binding flagged
python examples/tools/barrier_check.py examples/good/good_seq.json
# expected: SAFE — 0 hazards
python examples/tools/barrier_check.py examples/bad/bad_missing_barrier_seq.json
# expected: UNSAFE — missing barrier flagged
python examples/tools/barrier_check.py examples/bad/bad_wrong_mask_seq.json
# expected: UNSAFE — barrier present but masks do not cover the consumer
```

Target (Vulkan SDK host; compile commands recorded in `evals/README.md`):

```
glslangValidator -V examples/good/compute.comp -o examples/good/compute.spv
glslangValidator -V examples/bad/binding_mismatch.comp -o examples/bad/binding_mismatch.spv
glslangValidator -V examples/bad/missing_barrier.comp -o examples/bad/missing_barrier.spv
```

This machine has no Vulkan SDK / glslangValidator, so the `.comp` and `.c` files are
TARGET-ONLY (researched, not tool-verified). The python models above were actually
run; their real output is recorded in `evals/README.md`.

## Where the knowledge comes from

- Vulkan 1.x specification (https://docs.vulkan.org/spec/latest/), Vulkan compute pipeline chapter
- SPIR-V specification (https://registry.khronos.org/SPIR-V/)
- glslangValidator (https://github.com/KhronosGroup/glslang)
- Vulkan Tutorial — compute shaders (https://vulkan-tutorial.com/Compute_Shader)
- Kronos Vulkan samples (https://github.com/KhronosGroup/Vulkan-Samples)

## Related skills

- `ptx-assembly` — NVIDIA PTX layer; CUDA-only, the SPIR-V/Vulkan equivalent.
- `gpu-memory-model-coherence` — coherence scope and visibility rules shared by
  compute pipelines.
- `gpu-kernel-verification-beyond-oracle` — verifying kernel correctness beyond fixed
  oracles; applies to compute shaders as well.
- `gpu-kernel-reward-hacking-detection` — adversarial detection for training kernels.
- `performance-measurement-discipline` — measure dispatch throughput without conflating
  it with correctness.

## Evaluation

Synthetic: binding mismatch between shader and host layout (caught by
`binding_check.py`), missing or wrong-mask barriers between dispatches (caught by
`barrier_check.py`), wrong global-index math, hand-written SPIR-V.
False-positive: a correct shader/host mirror and correct narrow barrier masks must NOT
be "strengthened" to `ALL_COMMANDS`/`MEMORY_READ` or flagged as missing.
Historical: validation-layer-only-catchable binding bugs; the barrier-free compute
race class. Adversarial: a pipeline that "works" by luck on one GPU but is
unsynchronized. Full catalog: `evals/README.md`.
