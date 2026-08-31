#pragma once

#include <dunya/gpu/texture/texture.h>
#include <dunya/gpu/uploader/uploader.h>
#include <dunya/core/config/config.h>
#include <dunya/gpu/device/device.h>
#include <dunya/field/sampledsdf/sampledsdf.h>

#include <vulkan/vulkan.h>
#include <optional>
#include <array>
#include <cstddef>
#include <span>
#include <unordered_map>
#include <vector>

namespace dunya::renderer {

struct VolumeImages {
  const dunya::gpu::Image& distance;
  const dunya::gpu::Image& material;
};

struct VolumeKey {
  std::vector<std::byte> bytes;

  bool operator==(const VolumeKey& other) const = default;
};

VolumeKey volumeKey(
  std::span<const dunya::field::Primitive> primitives,
  const glm::uvec3& resolution,
  float margin
);

class VolumePool {
public:
  explicit VolumePool(const dunya::gpu::Device& device);

  VolumePool(const VolumePool&) = delete;
  VolumePool& operator=(const VolumePool&) = delete;

  VolumePool(VolumePool&& other) = default;
  VolumePool& operator=(VolumePool&& other) = default;

  [[nodiscard]] uint32_t acquire(const VolumeKey& key);

  [[nodiscard]] uint32_t allocate(const dunya::field::SampledSdf& grid);

  [[nodiscard]] uint32_t allocate(
    const dunya::field::SampledSdf& grid,
    const VolumeKey& key
  );

  [[nodiscard]] uint32_t makeUnique(
    dunya::gpu::Uploader& uploader,
    uint32_t index,
    const dunya::field::SampledSdf& grid
  );

  void retain(uint32_t index);

  void release(uint32_t index);

  [[nodiscard]] uint32_t users(uint32_t index) const;

  [[nodiscard]] uint32_t allocated() const;

  void upload(
    dunya::gpu::Uploader& uploader,
    uint32_t index,
    const dunya::field::SampledSdf& grid,
    const dunya::field::SampleBox& box
  );

  VolumeImages images(uint32_t index) const;

private:
  struct Volume {
    dunya::gpu::Texture distance;
    dunya::gpu::Texture material;
    uint32_t users = 0u;
    VolumeKey key;
  };

  [[nodiscard]] uint32_t fill(
    const dunya::field::SampledSdf& grid,
    VolumeKey key
  );

  void writeInto(
    dunya::gpu::Uploader& uploader,
    uint32_t index,
    const dunya::field::SampledSdf& grid,
    const dunya::field::SampleBox& box,
    VkImageLayout from
  );

  dunya::gpu::Texture makeDistanceVolume(
    const dunya::gpu::Device& device,
    const dunya::field::SampledSdf& grid
  );
  dunya::gpu::Texture makeMaterialVolume(
    const dunya::gpu::Device& device,
    const dunya::field::SampledSdf& grid
  );

  const dunya::gpu::Device& m_device;

  std::array<std::optional<Volume>, dunya::core::MAX_SDF_VOLUMES> m_volumes;
  std::vector<uint32_t> m_freeIndices;
  std::unordered_map<size_t, std::vector<uint32_t>> m_shared;
};

}
