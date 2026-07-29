#include "renderer.ih"

Renderer::Renderer(
  const Device& device,
  FieldPass& fieldPass,
  const Pipeline& meshPipeline,
  const Pipeline& fieldPipeline,
  Descriptors& descriptors,
  const VkSurfaceKHR& surface,
  uint32_t imageCount
)
    : m_device(device.vkDevice()),
      m_fieldPass(fieldPass),
      m_meshPipeline(meshPipeline),
      m_fieldPipeline(fieldPipeline),
      m_descriptors(descriptors),
      m_graphicsQueue(device.graphicsQueue()),
      m_presentQueue(device.presentQueue()) {
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
  const Frame& frameContext
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

  const Pipeline& pipeline = (frameContext.mode == PipelineType::Mesh)
                               ? m_meshPipeline
                               : m_fieldPipeline;
  vkCmdBindPipeline(
    m_commandBuffers[m_currentFrame],
    VK_PIPELINE_BIND_POINT_GRAPHICS,
    pipeline.pipeline()
  );

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

  if (frameContext.mode == PipelineType::Field) {
    FieldPushConstants constants{
      glm::inverse(frameContext.proj * frameContext.view),
      frameContext.cameraPos,
      static_cast<uint32_t>(m_fieldPass.primitives().size())
    };

    vkCmdBindDescriptorSets(
      m_commandBuffers[m_currentFrame],
      VK_PIPELINE_BIND_POINT_GRAPHICS,
      pipeline.pipelineLayout(),
      0,
      1,
      &m_fieldPass.descriptorSet(),
      0,
      nullptr
    );

    vkCmdPushConstants(
      m_commandBuffers[m_currentFrame],
      pipeline.pipelineLayout(),
      VK_SHADER_STAGE_FRAGMENT_BIT,
      0,
      sizeof(FieldPushConstants),
      &constants
    );

    vkCmdDraw(m_commandBuffers[m_currentFrame], 3, 1, 0, 0);
  } else if (frameContext.mode == PipelineType::Mesh) {
    vkCmdBindDescriptorSets(
      m_commandBuffers[m_currentFrame],
      VK_PIPELINE_BIND_POINT_GRAPHICS,
      pipeline.pipelineLayout(),
      0,
      1,
      &m_descriptors.descriptorSets()[m_currentFrame],
      0,
      nullptr
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

      vkCmdPushConstants(
        m_commandBuffers[m_currentFrame],
        pipeline.pipelineLayout(),
        VK_SHADER_STAGE_VERTEX_BIT,
        0,
        sizeof(glm::mat4),
        &item.model
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
  } else {
    throw std::invalid_argument("Uknown pipeline type");
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
  const Frame& frameContext
) {
  vkWaitForFences(
    m_device,
    1,
    &m_inFlightFences[m_currentFrame],
    VK_TRUE,
    UINT64_MAX
  );

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

  vkResetFences(m_device, 1, &m_inFlightFences[m_currentFrame]);

  vkResetCommandBuffer(m_commandBuffers[m_currentFrame], 0);
  recordCommandBuffer(swapChain, frameContext);

  if (frameContext.mode == PipelineType::Mesh) {
    UniformBufferObject ubo{};
    ubo.view = frameContext.view;
    ubo.proj = frameContext.proj;

    m_descriptors.update(m_currentFrame, ubo);
  }

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
