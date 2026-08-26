#include "renderer/volumepool/volumepool.ih"

using dunya::field::SampledField;

VolumePool::VolumePool(const Device& device) : m_device(device) {
  m_freeIndices.reserve(MAX_FIELD_OBJECTS);

  for (uint32_t i = 0; i < MAX_FIELD_OBJECTS; ++i) {
    m_freeIndices.push_back(MAX_FIELD_OBJECTS - 1 - i);
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

Texture VolumePool::makeDistanceVolume(
  const Device& device,
  const SampledField& grid
) {
  // 32-bit for the first measurement, deliberately: half's spacing near a
  // distance of one is about 0.001, which is the march epsilon
  return Texture(
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

Texture VolumePool::makeMaterialVolume(
  const Device& device,
  const SampledField& grid
) {
  std::vector<uint8_t> ids;
  ids.reserve(grid.materials.size());

  for (uint32_t material : grid.materials) {
    ids.push_back(static_cast<uint8_t>(material));
  }

  return Texture(
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
