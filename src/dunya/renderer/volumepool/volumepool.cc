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
  m_volumes[index].reset();
  m_freeIndices.push_back(index);
}

VolumeImages VolumePool::images(uint32_t index) const {
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
