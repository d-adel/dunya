#pragma once

#include <dunya/gpu/depthimage/depthimage.h>
#include <dunya/gpu/device/device.h>
#include <dunya/gpu/image/image.h>

#include <vulkan/vulkan.h>

namespace dunya::renderer {

class SceneTarget {
public:
  SceneTarget(
    const dunya::gpu::Device& device,
    VkFormat format,
    VkExtent2D extent,
    float scale
  );

  SceneTarget(const SceneTarget&) = delete;
  SceneTarget& operator=(const SceneTarget&) = delete;
  SceneTarget(SceneTarget&&) = delete;
  SceneTarget& operator=(SceneTarget&&) = delete;

  void resize(VkExtent2D extent);

  void setScale(float scale);

  [[nodiscard]] float scale() const noexcept;

  [[nodiscard]] VkExtent2D extent() const noexcept;

  [[nodiscard]] VkImage colourImage() const noexcept;

  [[nodiscard]] VkImageView colourView() const noexcept;

  [[nodiscard]] VkImageView depthView() const noexcept;

  [[nodiscard]] VkImage depthImage() const noexcept;

  void blitTo(
    VkCommandBuffer commands,
    VkImage destination,
    VkExtent2D destinationExtent
  ) const;

private:
  void build();

  const dunya::gpu::Device& m_device;

  VkFormat m_format;
  VkExtent2D m_windowExtent;
  VkExtent2D m_extent{};
  float m_scale;

  dunya::gpu::Image m_colour;
  dunya::gpu::DepthImage m_depth;
};

}
