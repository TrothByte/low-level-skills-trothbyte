# Vulkan Compute Deep Reference

Sources: Vulkan 1.x specification (compute pipeline chapter, descriptor set layouts,
synchronization), SPIR-V specification, Vulkan Tutorial compute shader chapter,
Khronos Vulkan samples. Claims marked [INFERRED] are not yet confirmed on this
machine (no Vulkan SDK; see `evals/README.md`).

Each entry: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE →
VERIFICATION → SOURCE.

## 1. GLSL → SPIR-V → shader module

- **RULE**: Compute shaders are written in GLSL/HLSL, compiled to SPIR-V (`.spv`) with
  `glslangValidator`/`shaderc`, loaded into a `VkShaderModule` via
  `vkCreateShaderModule`, and referenced by a pipeline with an entry point name.
- **WHY AI GETS IT WRONG**: agents try to hand-write SPIR-V instructions or skip the
  compile step, inventing opcodes and binary layouts that are wrong.
- **CORRECT REASONING**: SPIR-V is a binary format defined by the SPIR-V spec; the
  reliable path is `glslangValidator -V shader.comp -o shader.spv` and then
  `vkCreateShaderModule(device, &createInfo{codeSize, pCode}, ...)` with the compiled
  bytes. `pCode` points at `uint32_t` words; `codeSize` must be a multiple of 4.
- **EXAMPLE** (bad): a pipeline create that feeds hand-authored "SPIR-V" integers.
- **COUNTEREXAMPLE** (good): `glslangValidator -V compute.comp -o compute.spv`, read
  the file into `uint32_t[]`, pass to `vkCreateShaderModule`.
- **VERIFICATION**: `glslangValidator -V <file>.comp -o <file>.spv` (target; not run
  on this machine). `spirv-dis` for inspection.
- **SOURCE**: SPIR-V specification; Vulkan spec chapter "Shader Modules".

## 2. VkComputePipeline creation and entry point

- **RULE**: `VkComputePipelineCreateInfo` needs `stage` (a
  `VkPipelineShaderStageCreateInfo{module, pName="main", stage=VK_SHADER_STAGE_COMPUTE_BIT}`)
  and `layout` (a `VkPipelineLayout`). Created with `vkCreateComputePipelines`.
- **WHY AI GETS IT WRONG**: the entry point name must match the shader's entry point
  exactly (`main`); agents pass the file name or a wrong stage enum.
- **CORRECT REASONING**: one shader module can feed many pipelines, but each pipeline
  names one entry point; for GLSL the entry is `main`. The pipeline layout is fixed at
  creation time — every descriptor set layout and push constant range it will ever use
  must already be in that `VkPipelineLayout`.
- **EXAMPLE** (bad): `pName = "compute_main"` while the GLSL entry is `void main()`.
- **COUNTEREXAMPLE** (good): `pName = "main"`, `stage = VK_SHADER_STAGE_COMPUTE_BIT`.
- **VERIFICATION**: validation layers report a missing entry point; pipeline creation
  returns `VK_ERROR_INVALID_SHADER_NV` style failures.
- **SOURCE**: Vulkan spec chapter "Compute Pipelines".

## 3. Descriptor set layouts: the mirror

- **RULE**: `VkDescriptorSetLayout` is an array of `VkDescriptorSetLayoutBinding`:
  `{binding, descriptorType, descriptorCount, stageFlags, pImmutableSamplers}`. The
  shader's `layout(set=S, binding=B, ...)` declarations must match these 1:1:
  same set index, same binding index, same descriptor type, `descriptorCount >=`
  shader array size, and the shader's stage in `stageFlags`.
- **WHY AI GETS IT WRONG**: this is the #1 silent failure. Shader and host bindings
  drift apart (off-by-one indices, storage vs uniform confusion, set index mixing) and
  the device silently reads garbage — no error unless `VK_LAYER_KHRONOS_validation`
  is enabled.
- **CORRECT REASONING**: treat the descriptor layout as a contract. Write a table
  mapping each shader `layout(set=, binding=)` to its host `VkDescriptorSetLayoutBinding`
  and cross-check type and count. A shader binding with no host counterpart, or a type
  mismatch (`buffer` block vs `std140 uniform` block), is a bug.
- **EXAMPLE** (bad): shader uses `layout(set=0, binding=1, std430) buffer OutBuf`
  while host declares binding 1 as `VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER`.
- **COUNTEREXAMPLE** (good): both say binding 1 = storage buffer, compute stage
  (see `examples/good/host_layout.json` + `examples/good/compute.comp`).
- **VERIFICATION**: `python examples/tools/binding_check.py <shader> <layout.json>`
  (host-verified) and validation layers at runtime (target).
- **SOURCE**: Vulkan spec chapters "Descriptor Set Layout" and "Resource Descriptors".

## 4. Descriptor pools, sets, writes

- **RULE**: descriptor sets are allocated from a `VkDescriptorPool` that reserves
  enough of each type (`maxSets`, per-type pool sizes). Sets are updated with
  `vkUpdateDescriptorSets` and `VkWriteDescriptorSet{descriptorType, dstSet, dstBinding,
  dstArrayElement, descriptorCount, pBufferInfo|pImageInfo|pTexelBufferView}`.
- **WHY AI GETS IT WRONG**: agents omit the pool sizing, write to the wrong set/binding
  number in `VkWriteDescriptorSet`, or leave `descriptorType` inconsistent with the
  layout.
- **CORRECT REASONING**: the pool must hold every set in flight (including per-frame
  double/triple buffering). Each `VkWriteDescriptorSet` must name the exact
  `dstSet`/`dstBinding`/`dstArrayElement` from the layout, and its `descriptorType`
  must equal the layout binding's type. `VkDescriptorBufferInfo{offset, range}` covers
  the exact byte region the shader reads/writes.
- **EXAMPLE** (bad): `VkWriteDescriptorSet` for a storage buffer that fills
  `pBufferInfo` with `range = VK_WHOLE_SIZE` while the shader expects a tight range.
- **COUNTEREXAMPLE** (good): `offset=0, range=sizeBytes` for each buffer, type
  `VK_DESCRIPTOR_TYPE_STORAGE_BUFFER`.
- **VERIFICATION**: validation layers (target); the host binding model (this repo).
- **SOURCE**: Vulkan spec chapters "Descriptor Pool", "Descriptor Sets",
  "Descriptor Set Updates".

## 5. Dispatch math: local_size vs dispatch count

- **RULE**: `vkCmdDispatch(groupCountX, groupCountY, groupCountZ)` launches that many
  workgroups; each workgroup runs `local_size` invocations. The shader reads
  `gl_WorkGroupID` (which workgroup), `gl_LocalInvocationID` (position inside), and
  `gl_WorkGroupSize` (= `local_size`). The global index is
  `gid = gl_WorkGroupID * gl_WorkGroupSize + gl_LocalInvocationID`.
- **WHY AI GETS IT WRONG**: agents conflate `local_size` with the dispatch count and
  compute `gid` from the dispatch count only, indexing buffers out of range or leaving
  elements unprocessed.
- **CORRECT REASONING**: total threads on an axis = `local_size_x * groupCountX`.
  To cover `n` elements with `local_size_x = L`, dispatch
  `groupCountX = ceil(n / L)` and bounds-check `gid < n` inside the shader. Use
  `gl_NumWorkGroups` when needed.
- **EXAMPLE** (bad): `n = 100`, `local_size_x = 64`, dispatch `vkCmdDispatch(1,1,1)` —
  only 64 threads, elements 64..99 never touched.
- **COUNTEREXAMPLE** (good): dispatch `vkCmdDispatch(2,1,1)` (ceil(100/64)) and guard
  `if (gid >= 100) return;` (see `examples/good/compute.comp`).
- **VERIFICATION**: bounds-check by code review; fuzz grid sizes with
  `gpu-kernel-verification-beyond-oracle` methodology.
- **SOURCE**: Vulkan spec chapter "Dispatch"; GLSL manual on built-in variables.

## 6. Same-queue visibility and the barrier requirement

- **RULE**: commands recorded in one command buffer on one queue execute in
  submission order, but memory writes are NOT implicitly visible to later commands.
  Making a write visible to a later read requires a synchronization operation
  (`vkCmdPipelineBarrier`, semaphore, or event) between them.
- **WHY AI GETS IT WRONG**: agents assume program order on the CPU implies device-side
  ordering; the classic bug is two dispatches in one command buffer where the second
  reads the first's output with no barrier — works by luck on some GPUs, reads stale
  data on others.
- **CORRECT REASONING**: for every buffer written by command A and read by later
  command B (same queue or different queue, compute or transfer), insert a barrier
  after A whose dst masks cover B's stage and access. The barrier's src masks must
  cover A's producing stage/access.
- **EXAMPLE** (bad): dispatch writes `scratch[]`, then `vkCmdCopyBuffer` reads it,
  with no barrier (see `examples/bad/missing_barrier.comp`).
- **COUNTEREXAMPLE** (good): `vkCmdPipelineBarrier` with src
  `COMPUTE|SHADER_WRITE`, dst `TRANSFER|TRANSFER_READ` between them
  (see `examples/good/good_seq.json`, `examples/good/pipeline_setup.c`).
- **VERIFICATION**: `python examples/tools/barrier_check.py <seq.json>` (host-verified);
  validation layers flag only some cases — race classes need reasoning.
- **SOURCE**: Vulkan spec chapter "Synchronization and Cache Control", "Memory Model".

## 7. vkCmdPipelineBarrier stage and access masks

- **RULE**: `vkCmdPipelineBarrier(srcStageMask, dstStageMask, dependencyFlags,
  memoryBarrierCount, pMemoryBarriers, bufferBarrierCount, pBufferMemoryBarriers,
  imageBarrierCount, pImageMemoryBarriers)`. Each `VkBufferMemoryBarrier` names a
  buffer, offset, size, and src/dst access masks. A barrier orders operations when
  `srcStage/access` covers the producing operations and `dstStage/access` covers the
  consuming operations.
- **WHY AI GETS IT WRONG**: `VK_PIPELINE_STAGE_ALL_COMMANDS_BIT` +
  `VK_ACCESS_MEMORY_READ_BIT`/`MEMORY_WRITE_BIT` are valid but coarse (they serialize
  everything and can hurt performance), while genuinely wrong masks (e.g.
  `VK_PIPELINE_STAGE_VERTEX_SHADER_BIT` for a compute producer) pass validation but
  break correctness.
- **CORRECT REASONING**: pick the narrowest masks that describe the actual producer
  and consumer. Compute producer: `srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT`,
  `srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT`. Compute consumer:
  `dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT`,
  `dstAccessMask = VK_ACCESS_SHADER_READ_BIT`. Transfer consumer adds
  `VK_PIPELINE_STAGE_TRANSFER_BIT` + `VK_ACCESS_TRANSFER_READ_BIT`.
- **EXAMPLE** (bad): barrier with `dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT` only,
  placed before a compute shader that reads the buffer.
- **COUNTEREXAMPLE** (good): a `VkBufferMemoryBarrier` for the out buffer after
  dispatch: src `COMPUTE|SHADER_WRITE`, dst `COMPUTE|SHADER_READ | TRANSFER|TRANSFER_READ`.
- **VERIFICATION**: `barrier_check.py` models the covering rule on stage/access sets
  (host-verified); GPU run under validation layers (target).
- **SOURCE**: Vulkan spec chapter "Synchronization and Cache Control".

## 8. Push constants

- **RULE**: push constants are a small fast block written with `vkCmdPushConstants`.
  Every range the shader's `push_constant` block uses must be declared in the
  `VkPipelineLayout` as `VkPushConstantRange{stageFlags, offset, size}`. `offset` and
  `size` must be multiples of 4; the device guarantees at least 128 bytes.
- **WHY AI GETS IT WRONG**: agents forget to declare the range in the layout, or
  declare a size smaller than the shader block, or push at an unaligned offset.
- **CORRECT REASONING**: the sum of declared ranges is what `vkCmdPushConstants`
  may write; an undeclared or oversized block is a validation error and can fail on
  some devices. Match the host range to the GLSL `layout(push_constant) uniform Block`
  fields (scalars are 4-byte aligned; a `uint n` block is `{offset 0, size 4}`).
- **EXAMPLE** (bad): GLSL block `{uint n; uint base;}` (8 bytes) but pipeline layout
  declares `{size = 4}`.
- **COUNTEREXAMPLE** (good): `VkPushConstantRange{STAGE_COMPUTE, 0, 8}` matching the
  8-byte block in `examples/good/compute.comp`.
- **VERIFICATION**: validation layers report mismatched push constant ranges (target).
- **SOURCE**: Vulkan spec chapters "Push Constant Ranges", "Push Constants".

## 9. Specialization constants

- **RULE**: `VkPipelineShaderStageCreateInfo.pSpecializationInfo` lets one SPIR-V
  module vary `layout(constant_id = N)` values (e.g. `local_size_x`, element count)
  at pipeline-creation time, without recompiling GLSL.
- **WHY AI GETS IT WRONG**: agents hardcode sizes in GLSL and then try to reuse one
  pipeline for many shapes, or misalign `constant_id` indices.
- **CORRECT REASONING**: declare `layout(local_size_x_id = 0, local_size_x = 64) in;`
  so the default is 64 and a specialization entry can override it. The
  `VkSpecializationMapEntry{constantID, offset, size}` must match the shader's
  `constant_id` values and the data type.
- **EXAMPLE** (bad): specialization entry for `constantID = 1` while the shader only
  declares id 0.
- **COUNTEREXAMPLE** (good): one compiled module, pipelines for `local_size = 64, 128, 256`.
- **VERIFICATION**: validation layers + pipeline creation success (target).
- **SOURCE**: Vulkan spec chapter "Specialization Constants".

## 10. Queue families, features, extensions, validation layers

- **RULE**: compute commands go to a queue from a queue family with the
  `VK_QUEUE_COMPUTE_BIT` flag (often the graphics family is also compute-capable).
  Optional features (e.g. `descriptorIndexing`, `runtimeDescriptorArray`) must be
  enabled at device creation. `VK_LAYER_KHRONOS_validation` is enabled at instance
  creation and reports binding/sync misuse at runtime.
- **WHY AI GETS IT WRONG**: agents submit to the graphics-only queue, forget to enable
  `VK_KHRONOS_validation`, or use descriptor-indexing features without enabling them.
- **CORRECT REASONING**: query `vkGetPhysicalDeviceQueueFamilyProperties`, pick a
  family whose flags include `VK_QUEUE_COMPUTE_BIT` (and `VK_QUEUE_TRANSFER_BIT` if
  needed). Enable the validation layer in `VkInstanceCreateInfo.ppEnabledLayerNames`
  and treat its output as authoritative. Every `VkPhysicalDeviceFeature` the shader
  relies on must be in `VkDeviceCreateInfo.pEnabledFeatures`.
- **EXAMPLE** (bad): binding a `VK_DESCRIPTOR_TYPE_STORAGE_BUFFER` array without
  `descriptorIndexing` enabled.
- **COUNTEREXAMPLE** (good): features enabled, validation layer on, compute queue
  family chosen (see `examples/good/pipeline_setup.c`).
- **VERIFICATION**: `vulkaninfo` (target); validation layer output (target).
- **SOURCE**: Vulkan spec chapters "Queue Family Properties", "Feature Requirements",
  "Validation Layers".
