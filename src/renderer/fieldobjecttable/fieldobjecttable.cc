#include "fieldobjecttable.ih"

FieldObjectTable::FieldObjectTable(const Device& device)
    : m_group(
        device,
        MAX_FRAMES_IN_FLIGHT,
        {{ENTRIES,
          MAX_FIELD_OBJECTS * sizeof(FieldObjectGPU),
          VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
            | VK_SHADER_STAGE_COMPUTE_BIT,
          DescriptorGroup::BufferUpdate::PerFrame,
          VK_DESCRIPTOR_TYPE_STORAGE_BUFFER},
         {PRIMITIVE_POOL,
          MAX_PRIMITIVE_POOL * sizeof(dunya::field::Primitive),
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
      ) {
  m_gpuFieldObjects.resize(MAX_FIELD_OBJECTS);
}

void FieldObjectTable::registerVolume(
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

void FieldObjectTable::newFrame() {
  m_bakeList.clear();
}

void FieldObjectTable::appendToBakeList(ObjectId id) {
  m_bakeList.push_back(id);
}

void FieldObjectTable::makeGPUField(
  ObjectId objectIndex,
  uint32_t primitiveOffset,
  uint32_t primitiveCount,
  const FieldObject& fieldObject,
  uint32_t fieldRepresentation
) {
  m_gpuFieldObjects[objectIndex] = fromFieldObject(
    fieldObject,
    primitiveOffset,
    primitiveCount,
    fieldRepresentation
  );
}

void FieldObjectTable::update(uint32_t frame) {
  std::span<const FieldObjectGPU> objects(m_gpuFieldObjects);

  m_group.write(ENTRIES, frame, objects.data(), objects.size_bytes());
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

std::span<const FieldObjectGPU> FieldObjectTable::
  gpuFieldObjects() const noexcept {
  return m_gpuFieldObjects;
}

std::span<const ObjectId> FieldObjectTable::bakeList() const noexcept {
  return m_bakeList;
}

const FieldObjectGPU& FieldObjectTable::gpuFieldObject(
  ObjectId objectIndex
) const {
  return m_gpuFieldObjects.at(objectIndex);
}
