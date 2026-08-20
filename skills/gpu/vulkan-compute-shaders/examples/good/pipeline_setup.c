/*
 * GOOD: Vulkan compute pipeline setup — TARGET-ONLY (requires the Vulkan SDK;
 * not compiled on the authoring machine).
 *
 * Matches examples/good/compute.comp 1:1:
 *   set=0 binding=0 STORAGE_BUFFER  in_buf
 *   set=0 binding=1 STORAGE_BUFFER  out_buf
 *   set=0 binding=2 UNIFORM_BUFFER  uniforms
 *   push constant block { uint n; uint base; }  -> range offset 0, size 8
 *
 * Shows: shader module, descriptor set layout, pipeline layout with the push
 * constant range, VkComputePipelineCreateInfo, descriptor pool/set/write,
 * dispatch math (ceil(n/64) workgroups) and the post-dispatch barrier before
 * a transfer read (vkCmdCopyBuffer).
 */
#include <vulkan/vulkan.h>

#include <stdint.h>
#include <string.h>

static VkResult load_spv(const char* path, uint32_t** out, uint32_t* out_words) {
    /* read file into a uint32_t buffer (bytes / 4 words, file size multiple of 4) */
    (void)path;
    (void)out;
    (void)out_words;
    return VK_ERROR_UNKNOWN; /* placeholder: real code reads the .spv file */
}

VkResult create_compute_pipeline(
    VkDevice device, VkShaderModule module, VkPipelineLayout* out_layout,
    VkPipeline* out_pipeline) {
    VkResult r;

    /* ---- 1. Descriptor set layout: mirror of the shader bindings ---- */
    VkDescriptorSetLayoutBinding bindings[3] = {
        {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
         VK_SHADER_STAGE_COMPUTE_BIT, NULL},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
         VK_SHADER_STAGE_COMPUTE_BIT, NULL},
        {2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
         VK_SHADER_STAGE_COMPUTE_BIT, NULL},
    };
    VkDescriptorSetLayoutCreateInfo dslci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 3,
        .pBindings = bindings,
    };
    VkDescriptorSetLayout set_layout;
    r = vkCreateDescriptorSetLayout(device, &dslci, NULL, &set_layout);
    if (r != VK_SUCCESS) return r;

    /* ---- 2. Pipeline layout: the set layout + the push constant range ---- */
    VkPushConstantRange pc_range = {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .offset = 0,
        .size = 8, /* { uint n; uint base; } — multiple of 4 */
    };
    VkPipelineLayoutCreateInfo plci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &set_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pc_range,
    };
    r = vkCreatePipelineLayout(device, &plci, NULL, out_layout);
    if (r != VK_SUCCESS) return r;

    /* ---- 3. Compute pipeline: stage (module + entry point) + layout ---- */
    VkPipelineShaderStageCreateInfo stage = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = module,
        .pName = "main",
    };
    VkComputePipelineCreateInfo cpci = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = stage,
        .layout = *out_layout,
    };
    return vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &cpci, NULL,
                                    out_pipeline);
}

void record_dispatch_and_barrier(
    VkCommandBuffer cmd, VkPipeline pipeline, VkPipelineLayout layout,
    VkDescriptorSet set, uint32_t n, uint32_t local_size) {
    VkBufferMemoryBarrier buf_barrier = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask =
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT,
        .buffer = VK_NULL_HANDLE, /* set to out_buf handle */
        .offset = 0,
        .size = n * sizeof(float),
    };

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, 1,
                            &set, 0, NULL);
    vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, 8, NULL);

    /* dispatch count = ceil(n / local_size); local_size is the WORKGROUP size */
    uint32_t groups = (n + local_size - 1) / local_size;
    vkCmdDispatch(cmd, groups, 1, 1);

    /* Barrier: make the compute writes visible to a later compute read AND a
     * transfer read. src masks cover the producer (compute write), dst masks
     * cover the consumers (compute read, transfer read). */
    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,  /* srcStageMask */
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
            VK_PIPELINE_STAGE_TRANSFER_BIT,    /* dstStageMask */
        0, 0, NULL, 1, &buf_barrier, 0, NULL);
}

/* Usage sketch:
 *   load_spv("compute.spv", &words, &count);
 *   vkCreateShaderModule(device, &(VkShaderModuleCreateInfo){
 *       .sType=..., .codeSize=count*4, .pCode=words}, NULL, &module);
 *   pick a queue family with VK_QUEUE_COMPUTE_BIT; enable the validation layer
 *   VK_LAYER_KHRONOS_validation at instance creation.
 */
