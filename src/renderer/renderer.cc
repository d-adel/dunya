#include "renderer.ih"

Renderer::Renderer(
  const Device& device,
  FieldPass& fieldPass,
  FieldObjectTable& fieldObjectTable,
  const VolumePool& volumePool,
  FrameGlobals& frameGlobals,
  const Pipeline& meshPipeline,
  const Pipeline& fieldPipeline,
  const ResourceTable& resourceTable,
  const VkSurfaceKHR& surface,
  uint32_t imageCount
)
    : m_graphicsQueue(device.graphicsQueue()),
      m_presentQueue(device.presentQueue()),
      m_device(device.vkDevice()),
      m_meshPipeline(meshPipeline),
      m_fieldPipeline(fieldPipeline),
      m_resourceTable(resourceTable),
      m_fieldPass(fieldPass),
      m_fieldObjectTable(fieldObjectTable),
      m_volumePool(volumePool),
      m_frameGlobals(frameGlobals) {
  createCommandPool(device.physicalDevice(), surface);
  createCommandBuffer();
  createSyncObjects(imageCount);
}

Renderer::~Renderer() {
  vkDestroyCommandPool(m_device, m_commandPool, nullptr);
  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    vkDestroySemaphore(m_device, m_imageAvailableSemaphores[i], nullptr);

    vkDestroyFence(m_device, m_inFlightFences[i], nullptr);
  }

  for (auto semaphore : m_renderFinishedSemaphores) {
    vkDestroySemaphore(m_device, semaphore, nullptr);
  }
}

void Renderer::createCommandPool(
  const VkPhysicalDevice& physicalDevice,
  const VkSurfaceKHR& surface
) {
  QueueFamilyIndices queueFamilyIndices =
    findQueueFamilies(physicalDevice, surface);

  VkCommandPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

  if (
    vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool)
    != VK_SUCCESS
  ) {
    throw std::runtime_error("Failed to create command pool");
  }
}

void Renderer::createCommandBuffer() {
  m_commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool = m_commandPool;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount = 1;

  allocInfo.commandBufferCount = (uint32_t)m_commandBuffers.size();

  if (
    vkAllocateCommandBuffers(m_device, &allocInfo, m_commandBuffers.data())
    != VK_SUCCESS
  ) {
    throw std::runtime_error("failed to allocate command buffers!");
  }
}

void Renderer::createSyncObjects(uint32_t imageCount) {
  m_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
  m_renderFinishedSemaphores.resize(imageCount);
  m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

  VkFenceCreateInfo fenceInfo{};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  VkSemaphoreCreateInfo semaphoreInfo{};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    if (
      vkCreateFence(m_device, &fenceInfo, nullptr, &m_inFlightFences[i])
      != VK_SUCCESS
    ) {
      throw std::runtime_error("Failed to create fence for a frame");
    }

    if (
      vkCreateSemaphore(
        m_device,
        &semaphoreInfo,
        nullptr,
        &m_imageAvailableSemaphores[i]
      )
      != VK_SUCCESS
    ) {
      throw std::runtime_error(
        "Failed to create image available semaphore for a frame"
      );
    }
  }

  for (auto& semaphore : m_renderFinishedSemaphores) {
    if (
      vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &semaphore)
      != VK_SUCCESS
    ) {
      throw std::runtime_error(
        "Failed to create render finished semaphore for a frame"
      );
    }
  }
}

void Renderer::recordCommandBuffer(
  const SwapChain& swapChain,
  const Frame& frameContext,
  const std::function<void(VkCommandBuffer)>& onOverlay
) {
  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

  if (
    vkBeginCommandBuffer(m_commandBuffers[m_currentFrame], &beginInfo)
    != VK_SUCCESS
  ) {
    throw std::runtime_error("failed to begin recording command buffer!");
  }

  VkImageMemoryBarrier2 imageBarrier{};
  imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
  imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  imageBarrier.srcAccessMask = 0;
  imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  imageBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
  imageBarrier.image = swapChain.image(m_imageIndex);
  imageBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

  VkImageMemoryBarrier2 depthBarrier{};
  depthBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
  depthBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depthBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
  depthBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
  depthBarrier.srcAccessMask = 0;
  depthBarrier.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT
                              | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
  depthBarrier.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT
                               | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  depthBarrier.image = swapChain.depthImage().vkImage();
  depthBarrier.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
  depthBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  depthBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

  std::array<VkImageMemoryBarrier2, 2> barriers = {imageBarrier, depthBarrier};
  VkDependencyInfo dependencyInfo{};
  dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  dependencyInfo.imageMemoryBarrierCount = 2;
  dependencyInfo.pImageMemoryBarriers = barriers.data();

  vkCmdPipelineBarrier2(m_commandBuffers[m_currentFrame], &dependencyInfo);

  VkRenderingAttachmentInfo attachmentInfo{};
  attachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  attachmentInfo.imageView = swapChain.imageView(m_imageIndex);
  attachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  attachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  attachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  attachmentInfo.clearValue = {{{0.0f, 0.0f, 0.0f, 1.0f}}};  // Black

  VkRenderingAttachmentInfo depthAttachmentInfo{};
  depthAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  depthAttachmentInfo.imageView = swapChain.depthImage().image().imageView();
  depthAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
  depthAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depthAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depthAttachmentInfo.clearValue.depthStencil = {1.0f, 0};

  VkRenderingInfo renderingInfo{};
  renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
  renderingInfo.renderArea = {{0, 0}, swapChain.extent()};
  renderingInfo.layerCount = 1;
  renderingInfo.colorAttachmentCount = 1;
  renderingInfo.pColorAttachments = &attachmentInfo;
  renderingInfo.pDepthAttachment = &depthAttachmentInfo;

  vkCmdBeginRendering(m_commandBuffers[m_currentFrame], &renderingInfo);

  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = (float)swapChain.extent().width;
  viewport.height = (float)swapChain.extent().height;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(m_commandBuffers[m_currentFrame], 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = swapChain.extent();
  vkCmdSetScissor(m_commandBuffers[m_currentFrame], 0, 1, &scissor);

  bool drawMeshes = frameContext.mode == PipelineType::Mesh
                    || frameContext.mode == PipelineType::Both;
  bool drawField = frameContext.mode == PipelineType::Field
                   || frameContext.mode == PipelineType::Both;

  const std::array<VkDescriptorSet, 2> sharedSets = {
    m_frameGlobals.descriptorSet(m_currentFrame),
    m_resourceTable.descriptorSet(m_currentFrame)
  };

  vkCmdBindDescriptorSets(
    m_commandBuffers[m_currentFrame],
    VK_PIPELINE_BIND_POINT_GRAPHICS,
    m_meshPipeline.pipelineLayout(),
    0,
    static_cast<uint32_t>(sharedSets.size()),
    sharedSets.data(),
    0,
    nullptr
  );

  if (drawMeshes) {
    vkCmdBindPipeline(
      m_commandBuffers[m_currentFrame],
      VK_PIPELINE_BIND_POINT_GRAPHICS,
      m_meshPipeline.pipeline()
    );

    for (const auto& item : frameContext.drawItems) {
      assert(item.meshIndex < frameContext.meshes.size());

      VkBuffer vertexBuffers[] = {
        frameContext.meshes[item.meshIndex].vertexBuffer().buffer()
      };
      VkDeviceSize offsets[] = {0};
      vkCmdBindVertexBuffers(
        m_commandBuffers[m_currentFrame],
        0,
        1,
        vertexBuffers,
        offsets
      );
      vkCmdBindIndexBuffer(
        m_commandBuffers[m_currentFrame],
        frameContext.meshes[item.meshIndex].indexBuffer().buffer(),
        0,
        VK_INDEX_TYPE_UINT32
      );

      const PushConstants pushConstants{item.model, item.materialIndex};

      vkCmdPushConstants(
        m_commandBuffers[m_currentFrame],
        m_meshPipeline.pipelineLayout(),
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        offsetof(PushConstants, materialIndex)
          + sizeof(PushConstants::materialIndex),
        &pushConstants
      );

      vkCmdDrawIndexed(
        m_commandBuffers[m_currentFrame],
        static_cast<uint32_t>(frameContext.meshes[item.meshIndex].indexCount()),
        1,
        0,
        0,
        0
      );
    }
  }

  if (drawField) {
    m_fieldObjectTable.update(m_currentFrame, frameContext.sharedFieldObjects);
    m_fieldObjectTable.updatePrimitives(
      m_currentFrame,
      frameContext.primitives
    );

    // After the pool write, because the dispatch reads this frame's copy.
    uint32_t index = frameContext.sharedFieldObjects[0].resolutionVolumeIndex.w;
    VolumeImages images = m_volumePool.images(index);
    m_fieldPass.bakeIfDirty(
      m_currentFrame,
      static_cast<uint32_t>(frameContext.primitives.size()),
      frameContext.sharedFieldObjects.empty() ? 0u : index,
      images
    );

    vkCmdBindPipeline(
      m_commandBuffers[m_currentFrame],
      VK_PIPELINE_BIND_POINT_GRAPHICS,
      m_fieldPipeline.pipeline()
    );

    vkCmdBindDescriptorSets(
      m_commandBuffers[m_currentFrame],
      VK_PIPELINE_BIND_POINT_GRAPHICS,
      m_fieldPipeline.pipelineLayout(),
      2,
      1,
      &m_fieldObjectTable.descriptorSet(m_currentFrame),
      0,
      nullptr
    );

    vkCmdDraw(m_commandBuffers[m_currentFrame], 3, 1, 0, 0);
  }

  // Last, and inside the pass: the overlay draws over the finished scene, and
  // it is given a command buffer rather than being known about - the renderer
  // has no idea what ImGui is.
  if (onOverlay) {
    onOverlay(m_commandBuffers[m_currentFrame]);
  }

  vkCmdEndRendering(m_commandBuffers[m_currentFrame]);

  VkImageMemoryBarrier2 imageMemoryBarrier2Present{};
  imageMemoryBarrier2Present.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
  imageMemoryBarrier2Present.oldLayout =
    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  imageMemoryBarrier2Present.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  imageMemoryBarrier2Present.srcStageMask =
    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  imageMemoryBarrier2Present.srcAccessMask =
    VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
  imageMemoryBarrier2Present.dstStageMask = VK_PIPELINE_STAGE_2_NONE;
  imageMemoryBarrier2Present.dstAccessMask = 0;
  imageMemoryBarrier2Present.image = swapChain.image(m_imageIndex);
  imageMemoryBarrier2Present.subresourceRange =
    {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  imageMemoryBarrier2Present.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  imageMemoryBarrier2Present.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

  VkDependencyInfo dependencyInfoPresent{};
  dependencyInfoPresent.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  dependencyInfoPresent.imageMemoryBarrierCount = 1;
  dependencyInfoPresent.pImageMemoryBarriers = &imageMemoryBarrier2Present;

  vkCmdPipelineBarrier2(
    m_commandBuffers[m_currentFrame],
    &dependencyInfoPresent
  );

  if (vkEndCommandBuffer(m_commandBuffers[m_currentFrame]) != VK_SUCCESS) {
    throw std::runtime_error("failed to record command buffer!");
  }
}

bool Renderer::drawFrame(
  const SwapChain& swapChain,
  const Frame& frameContext,
  const std::function<void(VkCommandBuffer)>& onOverlay,
  const std::function<void(VkImage)>& onFrameReady
) {
  if (
    vkWaitForFences(
      m_device,
      1,
      &m_inFlightFences[m_currentFrame],
      VK_TRUE,
      UINT64_MAX
    )
    != VK_SUCCESS
  ) {
    throw std::runtime_error("Failed waiting for the in flight fence");
  }

  VkResult result = vkAcquireNextImageKHR(
    m_device,
    swapChain.handle(),
    UINT64_MAX,
    m_imageAvailableSemaphores[m_currentFrame],
    VK_NULL_HANDLE,
    &m_imageIndex
  );

  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    return true;
  } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    throw std::runtime_error("Failed to acquire swap chain image");
  }

  if (
    vkResetFences(m_device, 1, &m_inFlightFences[m_currentFrame]) != VK_SUCCESS
  ) {
    throw std::runtime_error("Failed to reset the in flight fence");
  }

  if (vkResetCommandBuffer(m_commandBuffers[m_currentFrame], 0) != VK_SUCCESS) {
    throw std::runtime_error("Failed to reset the command buffer");
  }

  const glm::mat4 cameraViewProj = frameContext.proj * frameContext.view;

  const CameraUniform camera{
    frameContext.view,
    frameContext.proj,
    cameraViewProj,
    glm::inverse(cameraViewProj),
    frameContext.cameraPos
  };

  m_frameGlobals.update(m_currentFrame, camera, frameContext.march);

  recordCommandBuffer(swapChain, frameContext, onOverlay);

  VkSubmitInfo2 submitInfo2{};
  submitInfo2.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;

  VkSemaphoreSubmitInfo waitSubmitInfo{};
  waitSubmitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
  waitSubmitInfo.semaphore = m_imageAvailableSemaphores[m_currentFrame];
  waitSubmitInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

  VkCommandBufferSubmitInfo commandBufferInfo{};
  commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
  commandBufferInfo.commandBuffer = m_commandBuffers[m_currentFrame];

  VkSemaphoreSubmitInfo signalSubmitInfo{};
  signalSubmitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
  signalSubmitInfo.semaphore = m_renderFinishedSemaphores.at(m_imageIndex);
  signalSubmitInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

  submitInfo2.waitSemaphoreInfoCount = 1;
  submitInfo2.pWaitSemaphoreInfos = &waitSubmitInfo;
  submitInfo2.commandBufferInfoCount = 1;
  submitInfo2.pCommandBufferInfos = &commandBufferInfo;
  submitInfo2.signalSemaphoreInfoCount = 1;
  submitInfo2.pSignalSemaphoreInfos = &signalSubmitInfo;

  if (
    vkQueueSubmit2(
      m_graphicsQueue,
      1,
      &submitInfo2,
      m_inFlightFences[m_currentFrame]
    )
    != VK_SUCCESS
  ) {
    throw std::runtime_error("failed to submit draw command buffer!");
  }

  // Still acquired, and the submit above is the work the reader wants to see,
  // so the wait belongs here rather than in the reader: it is this frame's
  // completion being waited for, which is the renderer's own business.
  if (onFrameReady) {
    if (vkDeviceWaitIdle(m_device) != VK_SUCCESS) {
      throw std::runtime_error("Failed waiting for the frame to be readable");
    }

    onFrameReady(swapChain.image(m_imageIndex));
  }

  VkSemaphore signalSemaphores[] = {
    m_renderFinishedSemaphores.at(m_imageIndex)
  };
  VkPresentInfoKHR presentInfo{};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = signalSemaphores;

  VkSwapchainKHR swapChains[] = {swapChain.handle()};
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = swapChains;
  presentInfo.pImageIndices = &m_imageIndex;
  presentInfo.pResults = nullptr;  // Optional

  result = vkQueuePresentKHR(m_presentQueue, &presentInfo);
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    return true;
  } else if (result != VK_SUCCESS) {
    throw std::runtime_error("Failed to present swap chain image");
  }

  m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

  return false;
}
