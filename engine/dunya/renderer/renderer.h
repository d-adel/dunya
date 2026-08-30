#pragma once

#include <vulkan/vulkan.h>
#include <dunya/gpu/swapchain/swapchain.h>
#include <dunya/gpu/pipeline/pipeline.h>
#include <dunya/renderer/resourcetable/resourcetable.h>
#include <dunya/renderer/frameglobals/frameglobals.h>
#include <dunya/renderer/frame/frame.h>
#include <dunya/renderer/sdfrecordtable/sdfrecordtable.h>
#include <dunya/renderer/volumepool/volumepool.h>
#include <dunya/renderer/sdfbaker/sdfbaker.h>

#include <dunya/core/config/config.h>

#include <functional>
#include <vector>

namespace dunya::renderer {

class Renderer {
public:
  Renderer(
    const dunya::gpu::Device& device,
    SdfRecordTable& sdfRecordTable,
    const SdfBaker& sdfBaker,
    const VolumePool& volumePool,
    FrameGlobals& frameGlobals,
    const dunya::gpu::Pipeline& meshPipeline,
    const dunya::gpu::Pipeline& sdfPipeline,
    const ResourceTable& resourceTable,
    const VkSurfaceKHR& surface,
    uint32_t imageCount
  );
  Renderer(Renderer const&) = delete;
  Renderer& operator=(Renderer const&) = delete;
  ~Renderer();

  [[nodiscard]]
  bool drawFrame(
    const dunya::gpu::SwapChain& swapChain,
    const Frame& frameContext,
    const std::function<void(VkCommandBuffer)>& onAfterScene = {},
    const std::function<void(VkImage)>& onFrameReady = {}
  );

private:
  void createCommandPool(
    const VkPhysicalDevice& physicalDevice,
    const VkSurfaceKHR& surface
  );
  void createCommandBuffer();

  void createSyncObjects(uint32_t imageCount);
  void recordCommandBuffer(
    const dunya::gpu::SwapChain& swapChain,
    const Frame& frameContext,
    const std::function<void(VkCommandBuffer)>& onAfterScene
  );

  VkCommandPool m_commandPool;
  std::vector<VkCommandBuffer> m_commandBuffers;
  std::vector<VkSemaphore> m_imageAvailableSemaphores;
  std::vector<VkSemaphore> m_renderFinishedSemaphores;
  std::vector<VkFence> m_inFlightFences;
  VkQueue m_graphicsQueue = VK_NULL_HANDLE;
  VkQueue m_presentQueue = VK_NULL_HANDLE;

  VkDevice m_device;
  uint32_t m_imageIndex = 0;
  uint32_t m_currentFrame = 0;

  const dunya::gpu::Pipeline& m_meshPipeline;
  const dunya::gpu::Pipeline& m_sdfPipeline;
  const ResourceTable& m_resourceTable;
  SdfRecordTable& m_recordTable;
  const SdfBaker& m_sdfBaker;
  const VolumePool& m_volumePool;
  FrameGlobals& m_frameGlobals;
};

}
