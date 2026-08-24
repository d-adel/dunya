#pragma once

#include "config/config.h"
#include "descriptorgroup/descriptorgroup.h"
#include "device/device.h"
#include "fieldobject/fieldobject.h"

#include <cstdint>
#include <span>

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

  explicit FieldObjectTable(const Device& device);

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
    ObjectId objectIndex,
    uint32_t primitiveOffset,
    uint32_t primitiveCount,
    const FieldObject& fieldObject,
    uint32_t fieldRepresentation
  );

  void update(uint32_t frame);

  void updatePrimitives(
    uint32_t frame,
    std::span<const dunya::field::Primitive> primitives
  );

  void newFrame();

  void appendToBakeList(ObjectId id);

  std::span<const FieldObjectGPU> gpuFieldObjects() const noexcept;
  std::span<const ObjectId> bakeList() const noexcept;

  const FieldObjectGPU& gpuFieldObject(ObjectId objectIndex) const;

private:
  std::vector<FieldObjectGPU> m_gpuFieldObjects;
  std::vector<ObjectId> m_bakeList;
  DescriptorGroup m_group;
};
