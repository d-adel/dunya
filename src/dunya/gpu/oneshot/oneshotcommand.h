#pragma once

#include <dunya/gpu/device/device.h>

#include <vulkan/vulkan.h>

namespace dunya::gpu {

class OneShotCommand {
public:
  OneShotCommand() = default;
  ~OneShotCommand();

  void start(const Device& device);
  void submit(const Device& device) const;

  VkCommandBuffer cmdBuffer() const noexcept;

private:
  VkCommandPool m_commandPool;
  VkCommandBuffer m_commandBuffer;
};

}  // namespace dunya::gpu
