#pragma once

#include <dunya/core/config/config.h>
#include <dunya/gpu/buffer/buffer.h>
#include <dunya/gpu/descriptorgroup/descriptorgroup.h>
#include <dunya/gpu/device/device.h>
#include <dunya/gpu/uploader/uploader.h>
#include <dunya/field/sampled/sampled.h>
#include <dunya/renderer/fieldrecord/fieldrecord.h>
#include <dunya/renderer/shadowgrid/shadowgrid.h>

#include <cstdint>
#include <span>

namespace dunya::renderer {

class FieldRecordTable {
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

  explicit FieldRecordTable(const dunya::gpu::Device& device);

  FieldRecordTable(const FieldRecordTable&) = delete;
  FieldRecordTable& operator=(const FieldRecordTable&) = delete;
  FieldRecordTable(FieldRecordTable&&) = delete;
  FieldRecordTable& operator=(FieldRecordTable&&) = delete;

  ~FieldRecordTable() = default;

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
    uint32_t fieldRepresentation
  );

  void update(uint32_t frame, uint32_t liveRecords, const glm::vec3& toLight);

  void updatePrimitives(
    uint32_t frame,
    std::span<const dunya::field::Primitive> primitives
  );

  void newFrame();

  void appendToBakeList(uint32_t slot);

  std::span<const FieldRecord> records() const noexcept;
  std::span<const uint32_t> bakeList() const noexcept;

  const FieldRecord& record(uint32_t recordIndex) const;

  void uploadBounds(
    dunya::gpu::Uploader& uploader,
    uint32_t volumeIndex,
    const dunya::field::SampledField& field
  );

  void uploadBounds(
    uint32_t volumeIndex,
    const dunya::field::SampledField& field
  );

  const dunya::gpu::Buffer& brickBounds() const noexcept;

private:
  [[nodiscard]] dunya::gpu::Buffer stageBounds(
    const dunya::field::SampledField& field,
    VkDeviceSize& sizeBytes
  ) const;

  void recordBounds(
    VkCommandBuffer commandBuffer,
    const dunya::gpu::Buffer& staging,
    uint32_t volumeIndex,
    VkDeviceSize sizeBytes
  ) const;

  const dunya::gpu::Device& m_device;

  std::vector<FieldRecord> m_records;

  std::vector<RecordBounds> m_recordBounds;

  ShadowGrid m_shadowGrid;
  std::vector<uint32_t> m_bakeList;
  dunya::gpu::DescriptorGroup m_group;

  dunya::gpu::Buffer m_brickBounds;
};

}  // namespace dunya::renderer
