#pragma once

#include "swapchain/swapchain.h"
#include "shadermodule/shadermodule.h"
#include "vertex/vertex.h"

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <string>
#include <span>
#include <filesystem>

struct PushConstants {
  glm::mat4 model;
  uint32_t materialIndex;
  uint32_t objectIndex;
};

static_assert(
  offsetof(PushConstants, objectIndex) == 68,
  "The push constant block must match its declaration in both mesh shaders"
);

enum class PipelineType {
  Mesh,
  Field,
  Both,
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

  std::vector<VkDescriptorSetLayout> setLayouts{};
  std::vector<VkPushConstantRange> pushConstantRanges{};
};

class Pipeline {
public:
  Pipeline(
    PipelineType type,
    const VkDevice& device,
    const std::vector<VkDescriptorSetLayout>& setLayouts,
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
  void destroy() noexcept;

  static std::filesystem::file_time_type newestIncludeTime();

  VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
  VkPipeline m_pipeline = VK_NULL_HANDLE;

  VkDevice m_device = VK_NULL_HANDLE;
  VkFormat m_swapChainImageFormat;
  PipelineConfig m_config{};

  std::vector<VkDescriptorSetLayout> m_setLayouts;
  VkFormat m_depthImageFormat;
  PipelineType m_type;

  std::filesystem::file_time_type m_vertTime;
  std::filesystem::file_time_type m_fragTime;
  std::filesystem::file_time_type m_includeTime;
};
