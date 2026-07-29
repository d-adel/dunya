#pragma once

#include "swapchain/swapchain.h"
#include "descriptors/descriptors.h"
#include "vertex/vertex.h"

#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <span>
#include <filesystem>

enum class PipelineType {
  Mesh,
  Field,
  Count
};

struct PipelineConfig {
  std::string vert;
  std::string frag;
  std::string vertexShader;
  std::string fragmentShader;

  std::vector<VkVertexInputBindingDescription> bindingDescriptions{};
  std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};

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
    const VkDescriptorSetLayout& setLayout,
    const SwapChain& swapChain
  );
  Pipeline(Pipeline const&) = delete;
  Pipeline& operator=(Pipeline const&) = delete;
  ~Pipeline();

  const VkPipeline& pipeline() const noexcept;
  const VkPipelineLayout& pipelineLayout() const noexcept;

  void reload();
  bool sourcesChanged() const;

private:
  void makeConfig();
  VkPipeline buildPipeline();
  VkPipelineLayout buildPipelineLayout();

  void create();
  VkShaderModule createShaderModule(const std::vector<char>& code);
  VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
  VkPipeline m_pipeline = VK_NULL_HANDLE;

  VkDevice m_device = VK_NULL_HANDLE;
  VkFormat m_swapChainImageFormat;
  PipelineConfig m_config{};

  VkDescriptorSetLayout m_setLayout;
  VkFormat m_depthImageFormat;
  PipelineType m_type;

  std::filesystem::file_time_type m_vertTime;
  std::filesystem::file_time_type m_fragTime;
};
