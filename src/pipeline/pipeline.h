#pragma once

#include "swapchain/swapchain.h"
#include "descriptors/descriptors.h"

#include <vulkan/vulkan.h>
#include <vector>
#include <string>

class Pipeline {
public:
  Pipeline(
    const VkDevice& device,
    const Descriptors& descriptors,
    const SwapChain& swapChain
  );
  Pipeline(Pipeline const&) = delete;
  Pipeline& operator=(Pipeline const&) = delete;
  ~Pipeline();

  const VkPipeline& graphicsPipeline() const noexcept;
  const VkPipelineLayout& pipelineLayout() const noexcept;

private:
  void createGraphicsPipeline(
    const Descriptors& descriptors,
    const VkFormat& depthImageFormat
  );
  VkShaderModule createShaderModule(const std::vector<char>& code);
  VkPipelineLayout m_pipelineLayout;
  VkPipeline m_graphicsPipeline;

  VkDevice m_device = VK_NULL_HANDLE;
  VkFormat m_swapChainImageFormat;
};
