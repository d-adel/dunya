#pragma once

#include "device/device.h"

#include <GLFW/glfw3.h>

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
