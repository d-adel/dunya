#include <dunya/renderer/volumepool/volumepool.ih>

namespace dunya::renderer {

using dunya::field::SampledField;

namespace {

size_t hashKey(const VolumeKey& key) {
  size_t hash = 14695981039346656037ull;

  for (std::byte value : key.bytes) {
    hash ^= static_cast<size_t>(value);
    hash *= 1099511628211ull;
  }

  return hash;
}

}

VolumeKey volumeKey(
  std::span<const dunya::field::Primitive> primitives,
  const glm::uvec3& resolution
) {
  const uint32_t dimensions[3] = {resolution.x, resolution.y, resolution.z};

  VolumeKey key;
  key.bytes.resize(primitives.size_bytes() + sizeof(dimensions));

  if (!primitives.empty()) {
    memcpy(key.bytes.data(), primitives.data(), primitives.size_bytes());
  }

  memcpy(
    key.bytes.data() + primitives.size_bytes(),
    dimensions,
    sizeof(dimensions)
  );

  return key;
}

VolumePool::VolumePool(const dunya::gpu::Device& device) : m_device(device) {
  m_freeIndices.reserve(dunya::core::MAX_FIELD_VOLUMES);

  for (uint32_t i = 0; i < dunya::core::MAX_FIELD_VOLUMES; ++i) {
    m_freeIndices.push_back(dunya::core::MAX_FIELD_VOLUMES - 1 - i);
  }
}

uint32_t VolumePool::acquire(const VolumeKey& key) {
  if (key.bytes.empty()) {
    return UINT32_MAX;
  }

  const auto bucket = m_shared.find(hashKey(key));

  if (bucket == m_shared.end()) {
    return UINT32_MAX;
  }

  for (const uint32_t index : bucket->second) {
    if (m_volumes[index].has_value() && m_volumes[index]->key == key) {
      ++m_volumes[index]->users;

      return index;
    }
  }

  return UINT32_MAX;
}

uint32_t VolumePool::fill(const SampledField& grid, VolumeKey key) {
  if (m_freeIndices.empty()) {
    return UINT32_MAX;
  }

  const uint32_t index = m_freeIndices.back();
  m_freeIndices.pop_back();

  m_volumes[index].emplace(
    Volume{
      makeDistanceVolume(m_device, grid),
      makeMaterialVolume(m_device, grid),
      1u,
      std::move(key)
    }
  );

  return index;
}

uint32_t VolumePool::allocate(const SampledField& grid) {
  return fill(grid, VolumeKey{});
}

uint32_t VolumePool::allocate(const SampledField& grid, const VolumeKey& key) {
  const uint32_t shared = acquire(key);

  if (shared != UINT32_MAX) {
    return shared;
  }

  const uint32_t index = fill(grid, key);

  if (index != UINT32_MAX && !key.bytes.empty()) {
    m_shared[hashKey(key)].push_back(index);
  }

  return index;
}

uint32_t VolumePool::makeUnique(
  dunya::gpu::Uploader& uploader,
  uint32_t index,
  const SampledField& grid
) {
  if (index >= m_volumes.size() || !m_volumes[index].has_value()) {
    throw std::runtime_error("Splitting a volume slot that is not allocated");
  }

  if (m_volumes[index]->users == 1u) {
    return index;
  }

  if (m_freeIndices.empty()) {
    return UINT32_MAX;
  }

  const uint32_t fresh = m_freeIndices.back();
  m_freeIndices.pop_back();

  m_volumes[fresh].emplace(
    Volume{
      dunya::gpu::Texture(
        m_device,
        grid.resolution.x,
        grid.resolution.y,
        grid.resolution.z,
        VK_FORMAT_R32_SFLOAT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
      ),
      dunya::gpu::Texture(
        m_device,
        grid.resolution.x,
        grid.resolution.y,
        grid.resolution.z,
        VK_FORMAT_R8_UINT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
      ),
      1u,
      VolumeKey{}
    }
  );

  writeInto(
    uploader,
    fresh,
    grid,
    dunya::field::SampleBox{glm::uvec3(0u), grid.resolution},
    VK_IMAGE_LAYOUT_UNDEFINED
  );

  return fresh;
}

void VolumePool::retain(uint32_t index) {
  if (index >= m_volumes.size() || !m_volumes[index].has_value()) {
    throw std::runtime_error("Retaining a volume slot that is not allocated");
  }

  ++m_volumes[index]->users;
}

void VolumePool::release(uint32_t index) {
  if (index >= m_volumes.size() || !m_volumes[index].has_value()) {
    throw std::runtime_error("Releasing a volume slot that is not allocated");
  }

  Volume& volume = *m_volumes[index];

  if (volume.users > 1u) {
    --volume.users;

    return;
  }

  m_device.waitIdle();

  if (!volume.key.bytes.empty()) {
    const auto bucket = m_shared.find(hashKey(volume.key));

    if (bucket != m_shared.end()) {
      std::erase(bucket->second, index);

      if (bucket->second.empty()) {
        m_shared.erase(bucket);
      }
    }
  }

  m_volumes[index].reset();
  m_freeIndices.push_back(index);
}

uint32_t VolumePool::users(uint32_t index) const {
  if (index >= m_volumes.size() || !m_volumes[index].has_value()) {
    return 0u;
  }

  return m_volumes[index]->users;
}

uint32_t VolumePool::allocated() const {
  return dunya::core::MAX_FIELD_VOLUMES
         - static_cast<uint32_t>(m_freeIndices.size());
}

namespace {

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
  const dunya::field::SampleBox& box,
  VkImageLayout from
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

  target.recordTransition(
    commandBuffer,
    from,
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

  uploader.keep(std::move(staging));
}

}

void VolumePool::writeInto(
  dunya::gpu::Uploader& uploader,
  uint32_t index,
  const SampledField& grid,
  const dunya::field::SampleBox& box,
  VkImageLayout from
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
    box,
    from
  );

  const std::vector<uint8_t> ids = gather<uint8_t>(grid, box, grid.materials);

  stageInto(
    m_device,
    uploader,
    volume.material.image(),
    ids.data(),
    ids.size(),
    box,
    from
  );
}

void VolumePool::upload(
  dunya::gpu::Uploader& uploader,
  uint32_t index,
  const SampledField& grid,
  const dunya::field::SampleBox& box
) {
  writeInto(
    uploader,
    index,
    grid,
    box,
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
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
  return dunya::gpu::Texture(
    device,
    grid.resolution.x,
    grid.resolution.y,
    grid.resolution.z,
    VK_FORMAT_R8_UINT,
    grid.materials.data(),
    grid.materials.size(),
    VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
  );
}

}
