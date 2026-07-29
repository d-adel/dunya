#pragma once

#include "swapchain/swapchain.h"
#include "descriptors/descriptors.h"
#include "vertex/vertex.h"

#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <span>

enum class PipelineType {
  Mesh,
  Field,
  Count
};

struct PipelineConfig {
  std::string vertexShader;
  std::string fragmentShader;

  std::span<const VkVertexInputBindingDescription> bindingDescriptions{};
  std::span<const VkVertexInputAttributeDescription> attributeDescriptions{};

  VkCullModeFlags cullMode = VK_CULL_MODE_NONE;

  VkBool32 depthTestEnable = VK_TRUE;
  VkBool32 depthWriteEnable = VK_TRUE;

  uint32_t setLayoutCount = 0;
  VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;

  uint32_t pushConstantSize = 0;
  VkShaderStageFlags pushConstantStageFlags = VK_SHADER_STAGE_VERTEX_BIT;
};

class Pipeline {
public:
  Pipeline(
    PipelineType type,
    const VkDevice& device,
    const Descriptors& descriptors,
    const SwapChain& swapChain
  );
  Pipeline(Pipeline const&) = delete;
  Pipeline& operator=(Pipeline const&) = delete;
  ~Pipeline();

  const VkPipeline& pipeline() const noexcept;
  const VkPipelineLayout& pipelineLayout() const noexcept;

private:
  void create(
    PipelineType type,
    const Descriptors& descriptors,
    const VkFormat& depthImageFormat
  );
  VkShaderModule createShaderModule(const std::vector<char>& code);
  VkPipelineLayout m_pipelineLayout;
  VkPipeline m_pipeline;

  VkDevice m_device = VK_NULL_HANDLE;
  VkFormat m_swapChainImageFormat;
};
