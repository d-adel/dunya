#pragma once

#include <vulkan/vulkan.h>
#include "swapchain/swapchain.h"
#include "pipeline/pipeline.h"
#include "resourcetable/resourcetable.h"
#include "frameglobals/frameglobals.h"
#include "frame/frame.h"
#include "fieldpass/fieldpass.h"

#include "config/config.h"

#include <vector>

class Renderer {
public:
  Renderer(
    const Device& device,
    FieldPass& fieldPass,
    FrameGlobals& frameGlobals,
    const Pipeline& meshPipeline,
    const Pipeline& fieldPipeline,
    const ResourceTable& resourceTable,
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
  const ResourceTable& m_resourceTable;
  FieldPass& m_fieldPass;
  FrameGlobals& m_frameGlobals;
};
