#pragma once

#include <dunya/core/config/config.h>
#include <dunya/gpu/buffer/buffer.h>
#include <dunya/gpu/descriptorgroup/descriptorgroup.h>
#include <dunya/gpu/device/device.h>
#include <dunya/gpu/uploader/uploader.h>
#include <dunya/field/sampledsdf/sampledsdf.h>
#include <dunya/renderer/sdfrecord/sdfrecord.h>
#include <dunya/renderer/shadowgrid/shadowgrid.h>

#include <cstdint>
#include <span>

namespace dunya::renderer {

class SdfRecordTable {
public:
  static constexpr uint32_t ENTRIES = 0;
  static constexpr uint32_t DISTANCE_VOLUMES = 1;
  static constexpr uint32_t MATERIAL_VOLUMES = 2;
  static constexpr uint32_t PRIMITIVE_POOL = 3;
  static constexpr uint32_t DISTANCE_VOLUMES_STORAGE = 4;
  static constexpr uint32_t MATERIAL_VOLUMES_STORAGE = 5;
  static constexpr uint32_t BRICK_BOUNDS = 6;
  static constexpr uint32_t RECORD_BOUNDS = 7;
  static constexpr uint32_t SHADOW_CELLS = 8;
  static constexpr uint32_t SHADOW_INDICES = 9;
  static constexpr uint32_t SHADOW_GRID = 10;

  explicit SdfRecordTable(const dunya::gpu::Device& device);

  SdfRecordTable(const SdfRecordTable&) = delete;
  SdfRecordTable& operator=(const SdfRecordTable&) = delete;
  SdfRecordTable(SdfRecordTable&&) = delete;
  SdfRecordTable& operator=(SdfRecordTable&&) = delete;

  ~SdfRecordTable() = default;

  const VkDescriptorSet& descriptorSet(uint32_t frame) const noexcept;
  const VkDescriptorSetLayout& setLayout() const noexcept;

  void registerVolume(
    VkImageView distanceView,
    VkImageView materialView,
    uint32_t volumeIndex
  );

  void setRecord(
    uint32_t recordIndex,
    uint32_t primitiveOffset,
    uint32_t primitiveCount,
    const dunya::objectmodel::Pose& pose,
    const dunya::objectmodel::SdfGrid& grid,
    const dunya::objectmodel::BakedVolume& volume,
    uint32_t fieldRepresentation,
    std::span<const dunya::field::Primitive> primitives
  );

  void update(uint32_t frame, uint32_t liveRecords, const glm::vec3& toLight);

  void updatePrimitives(
    uint32_t frame,
    std::span<const dunya::field::Primitive> primitives
  );

  void newFrame();

  void appendToBakeList(uint32_t slot);

  std::span<const SdfRecord> records() const noexcept;
  std::span<const uint32_t> bakeList() const noexcept;
  std::span<const uint32_t> bakeDispatch() const noexcept;

  const SdfRecord& record(uint32_t recordIndex) const;

  void uploadBounds(
    dunya::gpu::Uploader& uploader,
    uint32_t volumeIndex,
    const dunya::field::SampledSdf& field
  );

  void uploadBounds(
    uint32_t volumeIndex,
    const dunya::field::SampledSdf& field
  );

  const dunya::gpu::Buffer& brickBounds() const noexcept;

private:
  [[nodiscard]] dunya::gpu::Buffer stageBounds(
    const dunya::field::SampledSdf& field,
    VkDeviceSize& sizeBytes
  ) const;

  void recordBounds(
    VkCommandBuffer commandBuffer,
    const dunya::gpu::Buffer& staging,
    uint32_t volumeIndex,
    VkDeviceSize sizeBytes
  ) const;

  const dunya::gpu::Device& m_device;

  std::vector<SdfRecord> m_records;

  std::vector<RecordBounds> m_recordBounds;

  ShadowGrid m_shadowGrid;
  std::vector<uint32_t> m_bakeList;
  std::vector<uint32_t> m_bakeDispatch;
  std::vector<uint8_t> m_bakeQueued;
  dunya::gpu::DescriptorGroup m_group;

  dunya::gpu::Buffer m_brickBounds;
};

}
