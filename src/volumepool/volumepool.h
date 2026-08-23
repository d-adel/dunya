#pragma once

#include "texture/texture.h"
#include "config/config.h"
#include "device/device.h"
#include "field/sampled.h"

#include <vulkan/vulkan.h>
#include <optional>
#include <array>
#include <vector>

struct VolumeImages {
  const Image& distance;
  const Image& material;
};

class VolumePool {
public:
  VolumePool(const Device& device);

  VolumePool(const VolumePool&) = delete;
  VolumePool& operator=(const VolumePool&) = delete;

  VolumePool(VolumePool&& other) = default;
  VolumePool& operator=(VolumePool&& other) = default;

  uint32_t allocate(const dunya::field::SampledField& grid);
  void release(uint32_t index);

  VolumeImages images(uint32_t index) const;

private:
  struct Volume {
    Texture distance;
    Texture material;
  };

  Texture makeDistanceVolume(
    const Device& device,
    const dunya::field::SampledField& grid
  );
  Texture makeMaterialVolume(
    const Device& device,
    const dunya::field::SampledField& grid
  );

  const Device& m_device;

  std::array<std::optional<Volume>, MAX_FIELD_OBJECTS> m_volumes;
  std::vector<uint32_t> m_freeIndices;
};
