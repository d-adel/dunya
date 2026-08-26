#pragma once

#include <dunya/gpu/device/device.h>

#include <vulkan/vulkan.h>

#include <string>
#include <vector>

/* Separate from Pipeline rather than a branch inside it: almost everything
 * PipelineConfig carries - vertex input, rasterisation, blending, depth,
 * dynamic state, colour attachment formats - has no meaning for compute, and a
 * type whose fields are half inapplicable teaches the reader the wrong shape.
 */

namespace dunya::gpu {

class ComputePipeline {
public:
  ComputePipeline(
    const VkDevice& device,
    const std::string& spirvPath,
    const std::vector<VkDescriptorSetLayout>& setLayouts,
    const std::vector<VkPushConstantRange>& pushConstantRanges = {}
  );

  ComputePipeline(const ComputePipeline&) = delete;
  ComputePipeline& operator=(const ComputePipeline&) = delete;
  ComputePipeline(ComputePipeline&&) = delete;
  ComputePipeline& operator=(ComputePipeline&&) = delete;

  ~ComputePipeline();

  const VkPipeline& pipeline() const noexcept;
  const VkPipelineLayout& pipelineLayout() const noexcept;

private:
  void destroy() noexcept;

  VkDevice m_device = VK_NULL_HANDLE;
  VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
  VkPipeline m_pipeline = VK_NULL_HANDLE;
};

}  // namespace dunya::gpu
