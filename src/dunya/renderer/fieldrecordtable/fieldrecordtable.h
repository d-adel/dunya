#pragma once

#include <dunya/core/config/config.h>
#include <dunya/gpu/buffer/buffer.h>
#include <dunya/gpu/descriptorgroup/descriptorgroup.h>
#include <dunya/gpu/device/device.h>
#include <dunya/renderer/fieldrecord/fieldrecord.h>

#include <cstdint>
#include <span>

namespace dunya::renderer {

class FieldRecordTable {
public:
  // Set 2's layout, shared with field-shader.frag and field-bake.comp. The
  // bake kept its 4/5, which is why the buffers are not consecutive.
  static constexpr uint32_t ENTRIES = 0;
  static constexpr uint32_t DISTANCE_VOLUMES = 1;
  static constexpr uint32_t MATERIAL_VOLUMES = 2;
  static constexpr uint32_t PRIMITIVE_POOL = 3;
  static constexpr uint32_t DISTANCE_VOLUMES_STORAGE = 4;
  static constexpr uint32_t MATERIAL_VOLUMES_STORAGE = 5;
  static constexpr uint32_t BRICK_BOUNDS = 6;

  explicit FieldRecordTable(const dunya::gpu::Device& device);

  FieldRecordTable(const FieldRecordTable&) = delete;
  FieldRecordTable& operator=(const FieldRecordTable&) = delete;
  FieldRecordTable(FieldRecordTable&&) = delete;
  FieldRecordTable& operator=(FieldRecordTable&&) = delete;

  ~FieldRecordTable() = default;

  const VkDescriptorSet& descriptorSet(uint32_t frame) const noexcept;
  const VkDescriptorSetLayout& setLayout() const noexcept;

  // Both views twice: sampled for the fragment shader, storage for the bake.
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
    const dunya::objectmodel::FieldGrid& grid,
    const dunya::objectmodel::BakedVolume& volume,
    uint32_t fieldRepresentation
  );

  void update(uint32_t frame);

  void updatePrimitives(
    uint32_t frame,
    std::span<const dunya::field::Primitive> primitives
  );

  void newFrame();

  void appendToBakeList(uint32_t slot);

  std::span<const FieldRecord> records() const noexcept;
  std::span<const uint32_t> bakeList() const noexcept;

  const FieldRecord& record(uint32_t recordIndex) const;

  // Exposed for the bake check, which reads a slot back and compares it with
  // the same reduction run on the CPU.
  const dunya::gpu::Buffer& brickBounds() const noexcept;

private:
  std::vector<FieldRecord> m_records;
  std::vector<uint32_t> m_bakeList;
  dunya::gpu::DescriptorGroup m_group;

  // Device-local because the compute pass writes it and the fragment shader
  // reads it: the CPU never touches these bytes. One fixed slot per volume
  // index, so the same number addresses an object's volumes and its bounds.
  dunya::gpu::Buffer m_brickBounds;
};

}  // namespace dunya::renderer
