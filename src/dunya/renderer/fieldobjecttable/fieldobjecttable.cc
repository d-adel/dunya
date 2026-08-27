#include "fieldobjecttable.ih"

namespace dunya::renderer {

namespace {

constexpr VkDeviceSize BRICK_BOUNDS_BYTES =
  static_cast<VkDeviceSize>(dunya::core::MAX_FIELD_OBJECTS)
  * dunya::core::MAX_BRICKS_PER_OBJECT * sizeof(float);

}  // namespace

FieldObjectTable::FieldObjectTable(const dunya::gpu::Device& device)
    : m_group(
        device,
        dunya::core::MAX_FRAMES_IN_FLIGHT,
        {{ENTRIES,
          dunya::core::MAX_FIELD_OBJECTS
            * sizeof(dunya::objectmodel::FieldObjectGPU),
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
          dunya::core::MAX_FIELD_OBJECTS},
         {MATERIAL_VOLUMES,
          VK_SHADER_STAGE_FRAGMENT_BIT,
          {},
          dunya::core::MAX_FIELD_OBJECTS}},
        {},
        {{DISTANCE_VOLUMES_STORAGE,
          VK_SHADER_STAGE_COMPUTE_BIT,
          {},
          dunya::core::MAX_FIELD_OBJECTS},
         {MATERIAL_VOLUMES_STORAGE,
          VK_SHADER_STAGE_COMPUTE_BIT,
          {},
          dunya::core::MAX_FIELD_OBJECTS}},
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
  m_gpuFieldObjects.resize(dunya::core::MAX_FIELD_OBJECTS);

  m_group.writeBuffer(BRICK_BOUNDS, m_brickBounds.buffer(), BRICK_BOUNDS_BYTES);
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

const dunya::gpu::Buffer& FieldObjectTable::brickBounds() const noexcept {
  return m_brickBounds;
}

void FieldObjectTable::newFrame() {
  m_bakeList.clear();
}

void FieldObjectTable::appendToBakeList(dunya::core::ObjectId id) {
  m_bakeList.push_back(id);
}

void FieldObjectTable::makeGPUField(
  dunya::core::ObjectId objectIndex,
  uint32_t primitiveOffset,
  uint32_t primitiveCount,
  const dunya::objectmodel::FieldObject& fieldObject,
  uint32_t fieldRepresentation
) {
  m_gpuFieldObjects[objectIndex] = dunya::objectmodel::fromFieldObject(
    fieldObject,
    primitiveOffset,
    primitiveCount,
    fieldRepresentation
  );
}

void FieldObjectTable::update(uint32_t frame) {
  std::span<const dunya::objectmodel::FieldObjectGPU> objects(
    m_gpuFieldObjects
  );

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

std::span<const dunya::objectmodel::FieldObjectGPU> FieldObjectTable::
  gpuFieldObjects() const noexcept {
  return m_gpuFieldObjects;
}

std::span<const dunya::core::ObjectId> FieldObjectTable::
  bakeList() const noexcept {
  return m_bakeList;
}

const dunya::objectmodel::FieldObjectGPU& FieldObjectTable::gpuFieldObject(
  dunya::core::ObjectId objectIndex
) const {
  return m_gpuFieldObjects.at(objectIndex);
}

}  // namespace dunya::renderer
