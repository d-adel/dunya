#include <dunya/renderer/volumepool/volumepool.ih>

namespace dunya::renderer {

using dunya::field::SampledField;

VolumePool::VolumePool(const dunya::gpu::Device& device) : m_device(device) {
  m_freeIndices.reserve(dunya::core::MAX_FIELD_VOLUMES);

  for (uint32_t i = 0; i < dunya::core::MAX_FIELD_VOLUMES; ++i) {
    m_freeIndices.push_back(dunya::core::MAX_FIELD_VOLUMES - 1 - i);
  }
}

uint32_t VolumePool::allocate(const SampledField& grid) {
  if (m_freeIndices.empty()) {
    return UINT32_MAX;
  }

  uint32_t index = m_freeIndices.back();
  m_freeIndices.pop_back();

  m_volumes[index].emplace(
    Volume{
      makeDistanceVolume(m_device, grid),
      makeMaterialVolume(m_device, grid)
    }
  );

  return index;
}

void VolumePool::release(uint32_t index) {
  if (index >= m_volumes.size() || !m_volumes[index].has_value()) {
    throw std::runtime_error("Releasing a volume slot that is not allocated");
  }

  // Frames in flight may still name these images, and destroying a VkImage a
  // submitted command buffer references is a use-after-free. Releases are rare
  // - only an undone create - so this waits rather than deferring, the same
  // trade the swapchain already makes when it recreates.
  m_device.waitIdle();

  m_volumes[index].reset();
  m_freeIndices.push_back(index);
}

namespace {

// One tightly packed copy of a box out of a lattice, so the copy region needs
// no row length or image height and the staging buffer is exactly the box.
template<typename T, typename Source>
std::vector<T> gather(
  const SampledField& grid,
  const dunya::field::SampleBox& box,
  const std::vector<Source>& from
) {
  std::vector<T> packed;
  packed.reserve(
    static_cast<size_t>(box.extent.x) * box.extent.y * box.extent.z
  );

  for (uint32_t z = 0; z < box.extent.z; ++z) {
    for (uint32_t y = 0; y < box.extent.y; ++y) {
      for (uint32_t x = 0; x < box.extent.x; ++x) {
        const glm::uvec3 at = box.minimum + glm::uvec3(x, y, z);
        const size_t index =
          at.x + grid.resolution.x * (at.y + grid.resolution.y * at.z);

        packed.push_back(static_cast<T>(from[index]));
      }
    }
  }

  return packed;
}

void stageInto(
  const dunya::gpu::Device& device,
  dunya::gpu::Uploader& uploader,
  dunya::gpu::Image& target,
  const void* data,
  VkDeviceSize sizeBytes,
  const dunya::field::SampleBox& box
) {
  dunya::gpu::Buffer staging(
    device,
    sizeBytes,
    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
  );

  void* mapped = nullptr;

  if (
    vkMapMemory(device.vkDevice(), staging.memory(), 0, sizeBytes, 0, &mapped)
    != VK_SUCCESS
  ) {
    throw std::runtime_error("Failed to map volume upload staging memory");
  }

  memcpy(mapped, data, static_cast<size_t>(sizeBytes));
  vkUnmapMemory(device.vkDevice(), staging.memory());

  const VkCommandBuffer commandBuffer = uploader.begin();

  // A deformable's volumes never enter the bake, so they sit where the
  // texture's own upload left them and go back there.
  target.recordTransition(
    commandBuffer,
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
  );

  target.recordCopyFrom(
    commandBuffer,
    staging,
    box.extent.x,
    box.extent.y,
    box.extent.z,
    VkOffset3D{
      static_cast<int32_t>(box.minimum.x),
      static_cast<int32_t>(box.minimum.y),
      static_cast<int32_t>(box.minimum.z)
    }
  );

  target.recordTransition(
    commandBuffer,
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
  );

  // The GPU reads this after submit returns, so it outlives the call.
  uploader.keep(std::move(staging));
}

}  // namespace

void VolumePool::upload(
  dunya::gpu::Uploader& uploader,
  uint32_t index,
  const SampledField& grid,
  const dunya::field::SampleBox& box
) {
  if (index >= m_volumes.size() || !m_volumes[index].has_value()) {
    throw std::runtime_error(
      "Uploading into a volume slot that is not allocated"
    );
  }

  const glm::uvec3 beyond = box.minimum + box.extent;

  if (glm::any(glm::greaterThan(beyond, grid.resolution))) {
    throw std::runtime_error("An upload must stay inside the lattice");
  }

  if (box.extent.x == 0u || box.extent.y == 0u || box.extent.z == 0u) {
    return;
  }

  Volume& volume = *m_volumes[index];

  const std::vector<float> distances = gather<float>(grid, box, grid.distances);

  stageInto(
    m_device,
    uploader,
    volume.distance.image(),
    distances.data(),
    distances.size() * sizeof(float),
    box
  );

  const std::vector<uint8_t> ids = gather<uint8_t>(grid, box, grid.materials);

  stageInto(
    m_device,
    uploader,
    volume.material.image(),
    ids.data(),
    ids.size(),
    box
  );
}

VolumeImages VolumePool::images(uint32_t index) const {
  if (index >= m_volumes.size() || !m_volumes[index].has_value()) {
    throw std::runtime_error("Reading a volume slot that is not allocated");
  }

  const Volume& volume = *m_volumes[index];

  return {volume.distance.image(), volume.material.image()};
}

dunya::gpu::Texture VolumePool::makeDistanceVolume(
  const dunya::gpu::Device& device,
  const SampledField& grid
) {
  // 32-bit for the first measurement, deliberately: half's spacing near a
  // distance of one is about 0.001, which is the march epsilon
  return dunya::gpu::Texture(
    device,
    grid.resolution.x,
    grid.resolution.y,
    grid.resolution.z,
    VK_FORMAT_R32_SFLOAT,
    grid.distances.data(),
    grid.distances.size() * sizeof(float),
    VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
  );
}

dunya::gpu::Texture VolumePool::makeMaterialVolume(
  const dunya::gpu::Device& device,
  const SampledField& grid
) {
  std::vector<uint8_t> ids;
  ids.reserve(grid.materials.size());

  for (uint32_t material : grid.materials) {
    ids.push_back(static_cast<uint8_t>(material));
  }

  return dunya::gpu::Texture(
    device,
    grid.resolution.x,
    grid.resolution.y,
    grid.resolution.z,
    VK_FORMAT_R8_UINT,
    ids.data(),
    ids.size(),
    VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
  );
}

}  // namespace dunya::renderer
