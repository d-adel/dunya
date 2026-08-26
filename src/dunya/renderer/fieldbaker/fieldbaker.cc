#include "fieldbaker.ih"

namespace dunya::renderer {

using dunya::field::SampledField;

namespace {

VkImageMemoryBarrier2 volumeBarrier(
  VkImage image,
  VkImageLayout from,
  VkImageLayout to
) {
  VkImageMemoryBarrier2 barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
  barrier.oldLayout = from;
  barrier.newLayout = to;
  barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  barrier.srcAccessMask =
    VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
  barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  barrier.dstAccessMask =
    VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
  barrier.image = image;
  barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

  return barrier;
}

void transitionVolumes(
  VkCommandBuffer cmd,
  VkImage distances,
  VkImage materials,
  VkImageLayout from,
  VkImageLayout to
) {
  const std::array<VkImageMemoryBarrier2, 2> barriers{
    volumeBarrier(distances, from, to),
    volumeBarrier(materials, from, to)
  };

  VkDependencyInfo dependency{};
  dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  dependency.imageMemoryBarrierCount = static_cast<uint32_t>(barriers.size());
  dependency.pImageMemoryBarriers = barriers.data();

  vkCmdPipelineBarrier2(cmd, &dependency);
}

}  // namespace

void FieldBaker::bake(
  const dunya::objectmodel::FieldObjectGPU& gpu,
  uint32_t frame,
  VolumeImages images
) const {
  const auto start = std::chrono::steady_clock::now();

  dunya::gpu::OneShotCommand cmd;
  cmd.start(m_device);

  transitionVolumes(
    cmd.cmdBuffer(),
    images.distance.image(),
    images.material.image(),
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    VK_IMAGE_LAYOUT_GENERAL
  );

  vkCmdBindPipeline(
    cmd.cmdBuffer(),
    VK_PIPELINE_BIND_POINT_COMPUTE,
    m_bakePipeline.pipeline()
  );

  vkCmdBindDescriptorSets(
    cmd.cmdBuffer(),
    VK_PIPELINE_BIND_POINT_COMPUTE,
    m_bakePipeline.pipelineLayout(),
    0,
    1,
    &m_table.descriptorSet(frame),
    0,
    nullptr
  );

  glm::uvec3 resolution(
    gpu.resolutionVolumeIndex.x,
    gpu.resolutionVolumeIndex.y,
    gpu.resolutionVolumeIndex.z
  );

  const BakeParams params{
    gpu.localOrigin,
    gpu.voxelSize,
    glm::uvec4(resolution, gpu.config.x),
    glm::uvec4(gpu.resolutionVolumeIndex.w, gpu.config.w, 0u, 0u)
  };

  vkCmdPushConstants(
    cmd.cmdBuffer(),
    m_bakePipeline.pipelineLayout(),
    VK_SHADER_STAGE_COMPUTE_BIT,
    0,
    sizeof(params),
    &params
  );

  const glm::uvec3 groups = (resolution + glm::uvec3(3u)) / glm::uvec3(4u);

  vkCmdDispatch(cmd.cmdBuffer(), groups.x, groups.y, groups.z);

  transitionVolumes(
    cmd.cmdBuffer(),
    images.distance.image(),
    images.material.image(),
    VK_IMAGE_LAYOUT_GENERAL,
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
  );

  cmd.submit(m_device);

  const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
    std::chrono::steady_clock::now() - start
  );

  std::cout << "field grid rebaked on GPU in " << elapsed.count() << " us  ("
            << gpu.config.x << " primitives)\n";
}

namespace {

// Pulls a whole volume back into host memory. Only ever called by the bake
// check, so it takes the simple route and waits for the copy.
std::vector<uint8_t> readVolume(
  const dunya::gpu::Device& device,
  const dunya::gpu::Image& image,
  const glm::uvec3& resolution,
  VkDeviceSize sizeBytes
) {
  dunya::gpu::Buffer readback(
    device,
    sizeBytes,
    VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
  );

  dunya::gpu::OneShotCommand cmd;
  cmd.start(device);

  const std::array<VkImageMemoryBarrier2, 1> toSource{volumeBarrier(
    image.image(),
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
  )};

  VkDependencyInfo dependency{};
  dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  dependency.imageMemoryBarrierCount = 1;
  dependency.pImageMemoryBarriers = toSource.data();

  vkCmdPipelineBarrier2(cmd.cmdBuffer(), &dependency);

  VkBufferImageCopy region{};
  region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  region.imageExtent = {resolution.x, resolution.y, resolution.z};

  vkCmdCopyImageToBuffer(
    cmd.cmdBuffer(),
    image.image(),
    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    readback.buffer(),
    1,
    &region
  );

  const std::array<VkImageMemoryBarrier2, 1> toRead{volumeBarrier(
    image.image(),
    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
  )};

  dependency.pImageMemoryBarriers = toRead.data();
  vkCmdPipelineBarrier2(cmd.cmdBuffer(), &dependency);

  cmd.submit(device);

  std::vector<uint8_t> bytes(static_cast<size_t>(sizeBytes));

  void* mapped = nullptr;
  if (
    vkMapMemory(device.vkDevice(), readback.memory(), 0, sizeBytes, 0, &mapped)
    != VK_SUCCESS
  ) {
    throw std::runtime_error("Failed to map the bake readback buffer");
  }

  std::memcpy(bytes.data(), mapped, static_cast<size_t>(sizeBytes));
  vkUnmapMemory(device.vkDevice(), readback.memory());

  return bytes;
}

}  // namespace

FieldBaker::FieldBaker(
  const dunya::gpu::Device& device,
  const FieldObjectTable& table
)
    : m_device(device),
      m_table(table),
      m_bakePipeline(
        device.vkDevice(),
        "shaders/field-bake.comp.spv",
        std::vector<VkDescriptorSetLayout>{table.setLayout()},
        std::vector<VkPushConstantRange>{
          {VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(BakeParams)}
        }
      ) {}

void FieldBaker::verifyBake(
  const dunya::objectmodel::FieldObject& fieldObject,
  std::span<const dunya::field::Primitive> primitives,
  VolumeImages images
) const {
  const dunya::field::Aabb box = dunya::objectmodel::gridBox(primitives);

  const SampledField reference = dunya::field::bake(
    primitives,
    box.minimum,
    box.maximum,
    fieldObject.resolution
  );

  const std::vector<uint8_t> distanceBytes = readVolume(
    m_device,
    images.distance,
    fieldObject.resolution,
    reference.distances.size() * sizeof(float)
  );

  const std::vector<uint8_t> materialBytes = readVolume(
    m_device,
    images.material,
    fieldObject.resolution,
    reference.materials.size()
  );

  const float* baked = reinterpret_cast<const float*>(distanceBytes.data());

  float worstDistance = 0.0f;
  size_t materialMismatches = 0;

  for (size_t i = 0; i < reference.distances.size(); ++i) {
    worstDistance =
      std::max(worstDistance, std::abs(baked[i] - reference.distances[i]));

    if (materialBytes[i] != static_cast<uint8_t>(reference.materials[i])) {
      ++materialMismatches;
    }
  }

  std::cout << "bake check  worst distance " << std::scientific << worstDistance
            << "  material mismatches " << materialMismatches << " of "
            << reference.distances.size() << '\n';
}

}  // namespace dunya::renderer
