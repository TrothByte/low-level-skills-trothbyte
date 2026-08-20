/*
 * BAD: host/shader binding mismatch — TARGET-ONLY (requires the Vulkan SDK;
 * not compiled on the authoring machine).
 *
 * The shader examples/bad/binding_mismatch.comp declares:
 *   set=0 binding=0 STORAGE_BUFFER  in_buf
 *   set=0 binding=1 UNIFORM_BUFFER  uniforms   <- HOST LAYOUT SAYS STORAGE_BUFFER
 *   set=0 binding=2 STORAGE_BUFFER  out_buf    <- HOST LAYOUT SAYS UNIFORM_BUFFER
 *   set=0 binding=3 image2D         tex        <- HOST LAYOUT HAS NO BINDING 3
 *
 * This host descriptor layout is the buggy side of the same contract
 * (compare with pipeline_setup.c which mirrors compute.comp correctly).
 * The shader compiles, the pipeline is created, the first dispatch "runs" —
 * and reads garbage, because the descriptor types bound here do not match what
 * the shader expects. No error is reported unless VK_LAYER_KHRONOS_validation
 * is enabled.
 */
#include <vulkan/vulkan.h>

static void bad_descriptor_layout(VkDevice device, VkDescriptorSetLayout* out) {
    /* Bug #1: binding 1 is UNIFORM_BUFFER here, but the shader's binding 1 is
     * a `buffer` block (STORAGE_BUFFER). */
    VkDescriptorSetLayoutBinding bindings[3] = {
        {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
         VK_SHADER_STAGE_COMPUTE_BIT, NULL},
        {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, /* BUG: should be STORAGE_BUFFER */
         VK_SHADER_STAGE_COMPUTE_BIT, NULL},
        {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, /* BUG: should be UNIFORM_BUFFER */
         VK_SHADER_STAGE_COMPUTE_BIT, NULL},
    };
    /* Bug #2: binding 3 used by the shader (`image2D tex`) is missing from the
     * layout entirely — the shader binds a descriptor that does not exist. */

    VkDescriptorSetLayoutCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 3,
        .pBindings = bindings,
    };
    (void)vkCreateDescriptorSetLayout(device, &info, NULL, out);
}

/* Detection (host, no GPU needed):
 *   python examples/tools/binding_check.py \
 *       examples/bad/binding_mismatch.comp examples/good/host_layout.json
 * must FAIL and name each mismatched (set, binding). */
