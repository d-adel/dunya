#pragma once

#include <dunya/gpu/buffer/buffer.h>
#include <dunya/gpu/device/device.h>
#include <dunya/gpu/pipeline/pipeline.h>
#include <dunya/gpu/swapchain/swapchain.h>
#include <dunya/view/grid/gridstyle.h>

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace dunya::gizmos {

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
    const dunya::view::GridPlane& plane = {},
    const dunya::view::GridStyle& style = {}
  );

  Grid(Grid const&) = delete;
  Grid& operator=(Grid const&) = delete;

  void update(const glm::vec3& cameraPosition);

  void record(VkCommandBuffer commands, VkDescriptorSet globals) const;

private:
  void rebuild();

  [[nodiscard]] float planeDistance(const glm::vec3& position) const;

  const dunya::gpu::Device& m_device;

  dunya::view::GridPlane m_plane;
  dunya::view::GridStyle m_style;

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
