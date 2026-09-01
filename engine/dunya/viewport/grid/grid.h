#pragma once

#include <dunya/gpu/buffer/buffer.h>
#include <dunya/gpu/device/device.h>
#include <dunya/gpu/pipeline/pipeline.h>
#include <dunya/gpu/swapchain/swapchain.h>

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace dunya::viewport {

struct GridPlane {
  glm::vec3 normal{0.0f, 1.0f, 0.0f};
  glm::vec3 axisU{1.0f, 0.0f, 0.0f};
  glm::vec3 axisV{0.0f, 0.0f, 1.0f};
};

struct GridStyle {
  glm::vec4 primary{0.56f, 0.56f, 0.56f, 0.5f};
  glm::vec4 secondary{0.38f, 0.38f, 0.38f, 0.5f};

  glm::vec4 axisColourU{0.96f, 0.20f, 0.32f, 1.0f};
  glm::vec4 axisColourV{0.16f, 0.55f, 0.96f, 1.0f};

  int32_t size = 200;
  int32_t steps = 8;

  float levelBias = -0.2f;
  int32_t levelMin = 0;
  int32_t levelMax = 2;
};

struct GridVertex {
  glm::vec3 position;
  float kind;

  static VkVertexInputBindingDescription bindingDescription();
  static std::vector<VkVertexInputAttributeDescription> attributeDescriptions();
};

static_assert(
  offsetof(GridVertex, position) == 0,
  "GridVertex must keep the stride the grid pipeline binds"
);
static_assert(
  offsetof(GridVertex, kind) == 16,
  "GridVertex must keep the stride the grid pipeline binds"
);
static_assert(
  sizeof(GridVertex) == 32,
  "GridVertex must keep the stride the grid pipeline binds"
);

class Grid {
public:
  Grid(
    const dunya::gpu::Device& device,
    const dunya::gpu::SwapChain& swapChain,
    std::vector<VkDescriptorSetLayout> setLayouts,
    const GridPlane& plane = {},
    const GridStyle& style = {}
  );

  Grid(Grid const&) = delete;
  Grid& operator=(Grid const&) = delete;

  void update(const glm::vec3& cameraPosition);

  void record(VkCommandBuffer commands, VkDescriptorSet globals) const;

private:
  void rebuild();

  [[nodiscard]] float planeDistance(const glm::vec3& position) const;

  const dunya::gpu::Device& m_device;

  GridPlane m_plane;
  GridStyle m_style;

  dunya::gpu::Pipeline m_pipeline;

  dunya::gpu::Buffer m_vertices;

  uint32_t m_vertexCount = 0;
  uint32_t m_capacity = 0;

  float m_level = 0.0f;
  float m_decimals = 0.0f;
  float m_floored = 0.0f;
  float m_radius = 0.0f;

  float m_centreU = 0.0f;
  float m_centreV = 0.0f;

  bool m_built = false;
};

}
