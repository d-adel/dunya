#include "fieldrecordtable.ih"

namespace dunya::renderer {

namespace {

constexpr VkDeviceSize BRICK_BOUNDS_BYTES =
  static_cast<VkDeviceSize>(dunya::core::MAX_FIELD_VOLUMES)
  * dunya::core::BRICK_TABLE_STRIDE * sizeof(float);

}  // namespace

FieldRecordTable::FieldRecordTable(const dunya::gpu::Device& device)
    : m_group(
        device,
        dunya::core::MAX_FRAMES_IN_FLIGHT,
        {{ENTRIES,
          dunya::core::MAX_FIELD_RECORDS * sizeof(FieldRecord),
          VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
            | VK_SHADER_STAGE_COMPUTE_BIT,
          dunya::gpu::DescriptorGroup::BufferUpdate::PerFrame,
          VK_DESCRIPTOR_TYPE_STORAGE_BUFFER},
         {PRIMITIVE_POOL,
          dunya::core::MAX_PRIMITIVE_POOL * sizeof(dunya::field::Primitive),
          VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
          dunya::gpu::DescriptorGroup::BufferUpdate::PerFrame,
          VK_DESCRIPTOR_TYPE_STORAGE_BUFFER}},
        {{DISTANCE_VOLUMES,
          VK_SHADER_STAGE_FRAGMENT_BIT,
          {},
          dunya::core::MAX_FIELD_VOLUMES},
         {MATERIAL_VOLUMES,
          VK_SHADER_STAGE_FRAGMENT_BIT,
          {},
          dunya::core::MAX_FIELD_VOLUMES}},
        {},
        {{DISTANCE_VOLUMES_STORAGE,
          VK_SHADER_STAGE_COMPUTE_BIT,
          {},
          dunya::core::MAX_FIELD_VOLUMES},
         {MATERIAL_VOLUMES_STORAGE,
          VK_SHADER_STAGE_COMPUTE_BIT,
          {},
          dunya::core::MAX_FIELD_VOLUMES}},
        {{BRICK_BOUNDS,
          VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT}}
      ),
      m_brickBounds(
        device,
        BRICK_BOUNDS_BYTES,
        // TRANSFER_SRC so the bake check can copy a slot back out.
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
      ) {
  m_records.resize(dunya::core::MAX_FIELD_RECORDS);

  m_group.writeBuffer(BRICK_BOUNDS, m_brickBounds.buffer(), BRICK_BOUNDS_BYTES);
}

void FieldRecordTable::registerVolume(
  VkImageView distanceView,
  VkImageView materialView,
  uint32_t volumeIndex
) {
  m_group.writeImage(DISTANCE_VOLUMES, volumeIndex, distanceView);
  m_group.writeImage(MATERIAL_VOLUMES, volumeIndex, materialView);
  m_group.writeStorageImage(
    DISTANCE_VOLUMES_STORAGE,
    volumeIndex,
    distanceView
  );
  m_group.writeStorageImage(
    MATERIAL_VOLUMES_STORAGE,
    volumeIndex,
    materialView
  );
}

const dunya::gpu::Buffer& FieldRecordTable::brickBounds() const noexcept {
  return m_brickBounds;
}

void FieldRecordTable::newFrame() {
  m_bakeList.clear();
}

void FieldRecordTable::appendToBakeList(uint32_t slot) {
  m_bakeList.push_back(slot);
}

void FieldRecordTable::setRecord(
  uint32_t recordIndex,
  uint32_t primitiveOffset,
  uint32_t primitiveCount,
  const dunya::objectmodel::Pose& pose,
  const dunya::objectmodel::FieldGrid& grid,
  const dunya::objectmodel::BakedVolume& volume,
  uint32_t fieldRepresentation
) {
  m_records[recordIndex] = makeFieldRecord(
    pose,
    grid,
    volume,
    primitiveOffset,
    primitiveCount,
    fieldRepresentation
  );
}

void FieldRecordTable::update(uint32_t frame) {
  std::span<const FieldRecord> objects(m_records);

  m_group.write(ENTRIES, frame, objects.data(), objects.size_bytes());
}

void FieldRecordTable::updatePrimitives(
  uint32_t frame,
  std::span<const dunya::field::Primitive> primitives
) {
  m_group.write(
    PRIMITIVE_POOL,
    frame,
    primitives.data(),
    primitives.size_bytes()
  );
}

const VkDescriptorSet& FieldRecordTable::descriptorSet(
  uint32_t frame
) const noexcept {
  return m_group.descriptorSet(frame);
}

const VkDescriptorSetLayout& FieldRecordTable::setLayout() const noexcept {
  return m_group.setLayout();
}

std::span<const FieldRecord> FieldRecordTable::records() const noexcept {
  return m_records;
}

std::span<const uint32_t> FieldRecordTable::bakeList() const noexcept {
  return m_bakeList;
}

const FieldRecord& FieldRecordTable::record(uint32_t recordIndex) const {
  return m_records.at(recordIndex);
}

}  // namespace dunya::renderer
