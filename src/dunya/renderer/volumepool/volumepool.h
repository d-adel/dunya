#pragma once

#include <dunya/gpu/texture/texture.h>
#include <dunya/gpu/uploader/uploader.h>
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
  explicit VolumePool(const dunya::gpu::Device& device);

  VolumePool(const VolumePool&) = delete;
  VolumePool& operator=(const VolumePool&) = delete;

  VolumePool(VolumePool&& other) = default;
  VolumePool& operator=(VolumePool&& other) = default;

  uint32_t allocate(const dunya::field::SampledField& grid);
  void release(uint32_t index);

  // Rewrites one box of an allocated slot from the CPU grid. This is the only
  // way a deformable's volume changes: its records never join the bake list,
  // because that dispatch fills from the primitives and would erase the dent.
  //
  // Recorded into the uploader rather than submitted, because this runs inside
  // a frame: the six submissions it used to take were six waits on a queue
  // holding two frames of rendering.
  void upload(
    dunya::gpu::Uploader& uploader,
    uint32_t index,
    const dunya::field::SampledField& grid,
    const dunya::field::SampleBox& box
  );

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
