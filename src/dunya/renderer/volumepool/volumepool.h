#pragma once

#include <dunya/gpu/texture/texture.h>
#include <dunya/core/config/config.h>
#include <dunya/gpu/device/device.h>
#include <dunya/field/sampled/sampled.h>

#include <vulkan/vulkan.h>
#include <optional>
#include <array>
#include <vector>

namespace dunya::renderer {

struct VolumeImages {
  const dunya::gpu::Image& distance;
  const dunya::gpu::Image& material;
};

class VolumePool {
public:
  VolumePool(const dunya::gpu::Device& device);

  VolumePool(const VolumePool&) = delete;
  VolumePool& operator=(const VolumePool&) = delete;

  VolumePool(VolumePool&& other) = default;
  VolumePool& operator=(VolumePool&& other) = default;

  uint32_t allocate(const dunya::field::SampledField& grid);
  void release(uint32_t index);

  VolumeImages images(uint32_t index) const;

private:
  struct Volume {
    dunya::gpu::Texture distance;
    dunya::gpu::Texture material;
  };

  dunya::gpu::Texture makeDistanceVolume(
    const dunya::gpu::Device& device,
    const dunya::field::SampledField& grid
  );
  dunya::gpu::Texture makeMaterialVolume(
    const dunya::gpu::Device& device,
    const dunya::field::SampledField& grid
  );

  const dunya::gpu::Device& m_device;

  std::array<std::optional<Volume>, dunya::core::MAX_FIELD_VOLUMES> m_volumes;
  std::vector<uint32_t> m_freeIndices;
};

}  // namespace dunya::renderer
