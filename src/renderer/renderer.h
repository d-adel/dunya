#pragma once

#include <vulkan/vulkan.h>
#include "swapchain/swapchain.h"
#include "pipeline/pipeline.h"
#include "descriptors/descriptors.h"
#include "ubo/ubo.h"
#include "frame/frame.h"

#include <vector>

constexpr int MAX_FRAMES_IN_FLIGHT = 2;

class Renderer {
public:
  Renderer(
    const Device& device,
    const Pipeline& meshPipeline,
    const Pipeline& fieldPipeline,
    Descriptors& descriptors,
    const VkSurfaceKHR& surface,
    uint32_t imageCount
  );
  Renderer(Renderer const&) = delete;
  Renderer& operator=(Renderer const&) = delete;
  ~Renderer();

  bool drawFrame(const SwapChain& swapChain, const Frame& frameContext);

private:
  void createCommandPool(
    const VkPhysicalDevice& physicalDevice,
    const VkSurfaceKHR& surface
  );
  void createCommandBuffer();

  void createSyncObjects(uint32_t imageCount);
  void recordCommandBuffer(
    const SwapChain& swapChain,
    const Frame& frameContext
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

  const Pipeline& m_meshPipeline;
  const Pipeline& m_fieldPipeline;
  Descriptors& m_descriptors;
};
