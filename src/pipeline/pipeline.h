#pragma once

#include "descriptors/descriptors.h"

#include <GLFW/glfw3.h>
#include <vector>
#include <string>

class Pipeline {
public:
  Pipeline(
    const VkDevice& device,
    const VkFormat& swapChainImageFormat,
    const Descriptors& descriptors,
    const VkFormat& depthImageFormat
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
