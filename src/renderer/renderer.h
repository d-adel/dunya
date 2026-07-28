#pragma once

#include <vulkan/vulkan.h>
#include "swapchain/swapchain.h"
#include "scene/scene.h"
#include "ubo/ubo.h"

#include <vector>

constexpr int MAX_FRAMES_IN_FLIGHT = 2;

class Renderer {
public:
  Renderer(
    const Device& device,
    const VkSurfaceKHR& surface,
    uint32_t imageCount
  );
  Renderer(Renderer const&) = delete;
  Renderer& operator=(Renderer const&) = delete;
  ~Renderer();

  bool drawFrame(
    const SwapChain& swapChain,
    Scene& scene,
    const UniformBufferObject& ubo
  );

private:
  void createCommandPool(
    const VkPhysicalDevice& physicalDevice,
    const VkSurfaceKHR& surface
  );
  void createCommandBuffer();

  void createSyncObjects(uint32_t imageCount);
  void recordCommandBuffer(
    const SwapChain& swapChain,
    const Scene& scene,
    const std::vector<VkDescriptorSet>& descriptorSets
  );

  VkCommandPool m_commandPool;
  std::vector<VkCommandBuffer> m_commandBuffers;
  std::vector<VkSemaphore> m_imageAvailableSemaphores;
  std::vector<VkSemaphore> m_renderFinishedSemaphores;
  std::vector<VkFence> m_inFlightFences;
  VkQueue m_graphicsQueue = VK_NULL_HANDLE;
  VkQueue m_presentQueue = VK_NULL_HANDLE;

  VkDevice m_device;
  uint32_t m_imageIndex;
  uint32_t m_currentFrame = 0;
};
