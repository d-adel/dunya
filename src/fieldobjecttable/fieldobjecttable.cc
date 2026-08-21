#include "fieldobjecttable.ih"

FieldObjectTable::FieldObjectTable(const Device& device)
    : m_group(
        device,
        MAX_FRAMES_IN_FLIGHT,
        {{ENTRIES,
          MAX_FIELD_OBJECTS * sizeof(FieldObjectShared),
          VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
          DescriptorGroup::BufferUpdate::PerFrame,
          VK_DESCRIPTOR_TYPE_STORAGE_BUFFER},
         {PRIMITIVE_POOL,
          MAX_PRIMITIVES * sizeof(dunya::field::Primitive),
          VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
          DescriptorGroup::BufferUpdate::PerFrame,
          VK_DESCRIPTOR_TYPE_STORAGE_BUFFER}},
        {{DISTANCE_VOLUMES,
          VK_SHADER_STAGE_FRAGMENT_BIT,
          {},
          MAX_FIELD_OBJECTS},
         {MATERIAL_VOLUMES,
          VK_SHADER_STAGE_FRAGMENT_BIT,
          {},
          MAX_FIELD_OBJECTS}},
        {},
        {{DISTANCE_VOLUMES_STORAGE,
          VK_SHADER_STAGE_COMPUTE_BIT,
          {},
          MAX_FIELD_OBJECTS},
         {MATERIAL_VOLUMES_STORAGE,
          VK_SHADER_STAGE_COMPUTE_BIT,
          {},
          MAX_FIELD_OBJECTS}}
      ) {}

uint32_t FieldObjectTable::registerVolume(
  VkImageView distanceView,
  VkImageView materialView
) {
  if (m_volumeCount >= MAX_FIELD_OBJECTS) {
    throw std::runtime_error(
      "Failed registering volume, FieldObject limit reached"
    );
  }

  m_group.writeImage(DISTANCE_VOLUMES, m_volumeCount, distanceView);
  m_group.writeImage(MATERIAL_VOLUMES, m_volumeCount, materialView);
  m_group.writeStorageImage(
    DISTANCE_VOLUMES_STORAGE,
    m_volumeCount,
    distanceView
  );
  m_group.writeStorageImage(
    MATERIAL_VOLUMES_STORAGE,
    m_volumeCount,
    materialView
  );

  const uint32_t index = m_volumeCount;
  ++m_volumeCount;

  return index;
}

void FieldObjectTable::update(
  uint32_t frame,
  std::span<const FieldObjectShared> sharedFieldObjs
) {
  m_group.write(
    ENTRIES,
    frame,
    sharedFieldObjs.data(),
    sharedFieldObjs.size_bytes()
  );
}

void FieldObjectTable::updatePrimitives(
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

const VkDescriptorSet& FieldObjectTable::descriptorSet(
  uint32_t frame
) const noexcept {
  return m_group.descriptorSet(frame);
}

const VkDescriptorSetLayout& FieldObjectTable::setLayout() const noexcept {
  return m_group.setLayout();
}
