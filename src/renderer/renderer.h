#pragma once

#include <GLFW/glfw3.h>
#include "swapchain/swapchain.h"
#include "buffer/buffer.h"
#include "pipeline/pipeline.h"
#include "vertex/vertex.h"
#include "descriptors/descriptors.h"
#include "depthimage/depthimage.h"
#include "mesh/mesh.h"
#include "ubo/ubo.h"

#include <vector>

const int MAX_FRAMES_IN_FLIGHT = 2;

class Renderer {
public:
  Renderer(
    const Device& device,
    const VkSurfaceKHR& surface,
    uint32_t imageCount,
    Mesh& mesh
  );
  Renderer(Renderer const&) = delete;
  Renderer& operator=(Renderer const&) = delete;
  ~Renderer();

  bool drawFrame(
    const SwapChain& swapChain,
    const Pipeline& pipeline,
    Descriptors& descriptors,
    const DepthImage& depthImage,
    const UniformBufferObject& ubo
  );

private:
  void createCommandPool(
    const VkPhysicalDevice& physicalDevice,
    const VkSurfaceKHR& surface
  );
  void createCommandBuffer();
  template<typename T>
  Buffer createBuffer(
    const Device& device,
    const std::vector<T>& objVec,
    VkBufferUsageFlags usage
  );
  void createSyncObjects(uint32_t imageCount);
  void recordCommandBuffer(
    const SwapChain& SwapChain,
    const DepthImage& depthImage,
    const Pipeline& pipeline,
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

  // std::vector<Mesh> m_meshes;
  Mesh m_mesh;
};
