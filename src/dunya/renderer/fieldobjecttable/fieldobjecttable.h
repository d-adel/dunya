#pragma once

#include <dunya/core/config/config.h>
#include <dunya/gpu/descriptorgroup/descriptorgroup.h>
#include <dunya/gpu/device/device.h>
#include <dunya/objectmodel/fieldobject/fieldobject.h>

#include <cstdint>
#include <span>

namespace dunya::renderer {

class FieldObjectTable {
public:
  // Set 2's layout, shared with field-shader.frag and field-bake.comp. The
  // bake kept its 4/5, which is why the buffers are not consecutive.
  static constexpr uint32_t ENTRIES = 0;
  static constexpr uint32_t DISTANCE_VOLUMES = 1;
  static constexpr uint32_t MATERIAL_VOLUMES = 2;
  static constexpr uint32_t PRIMITIVE_POOL = 3;
  static constexpr uint32_t DISTANCE_VOLUMES_STORAGE = 4;
  static constexpr uint32_t MATERIAL_VOLUMES_STORAGE = 5;

  explicit FieldObjectTable(const dunya::gpu::Device& device);

  FieldObjectTable(const FieldObjectTable&) = delete;
  FieldObjectTable& operator=(const FieldObjectTable&) = delete;
  FieldObjectTable(FieldObjectTable&&) = delete;
  FieldObjectTable& operator=(FieldObjectTable&&) = delete;

  ~FieldObjectTable() = default;

  const VkDescriptorSet& descriptorSet(uint32_t frame) const noexcept;
  const VkDescriptorSetLayout& setLayout() const noexcept;

  // Both views twice: sampled for the fragment shader, storage for the bake.
  void registerVolume(
    VkImageView distanceView,
    VkImageView materialView,
    uint32_t volumeIndex
  );

  void makeGPUField(
    dunya::core::ObjectId objectIndex,
    uint32_t primitiveOffset,
    uint32_t primitiveCount,
    const dunya::objectmodel::FieldObject& fieldObject,
    uint32_t fieldRepresentation
  );

  void update(uint32_t frame);

  void updatePrimitives(
    uint32_t frame,
    std::span<const dunya::field::Primitive> primitives
  );

  void newFrame();

  void appendToBakeList(dunya::core::ObjectId id);

  std::span<const dunya::objectmodel::FieldObjectGPU>
  gpuFieldObjects() const noexcept;
  std::span<const dunya::core::ObjectId> bakeList() const noexcept;

  const dunya::objectmodel::FieldObjectGPU& gpuFieldObject(
    dunya::core::ObjectId objectIndex
  ) const;

private:
  std::vector<dunya::objectmodel::FieldObjectGPU> m_gpuFieldObjects;
  std::vector<dunya::core::ObjectId> m_bakeList;
  dunya::gpu::DescriptorGroup m_group;
};

}  // namespace dunya::renderer
