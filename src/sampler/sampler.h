#pragma once

#include "device/device.h"

#include <vulkan/vulkan.h>

#include <utility>

class Sampler {
public:
  Sampler(const Device& device);

  Sampler(const Sampler&) = delete;
  Sampler& operator=(const Sampler&) = delete;

  Sampler(Sampler&& other) noexcept;
  Sampler& operator=(Sampler&& other) noexcept;

  ~Sampler();

  VkSampler handle() const noexcept;

private:
  void destroy() noexcept;

  VkSampler m_sampler = VK_NULL_HANDLE;
  VkDevice m_device = VK_NULL_HANDLE;
};
