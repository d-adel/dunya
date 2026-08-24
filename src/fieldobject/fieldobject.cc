#include "fieldobject.ih"

FieldObjectGPU fromFieldObject(
  const FieldObject& fieldObject,
  uint32_t primitiveOffset,
  uint32_t primitiveCount,
  uint32_t fieldRepresentation
) {
  FieldObjectGPU gpu{};

  gpu.inverseModel = fieldObject.inverseModel();
  gpu.model = glm::inverse(gpu.inverseModel);

  gpu.voxelSize = glm::vec4(fieldObject.voxelSize, FIELD_GRID_MARGIN);

  gpu.resolutionVolumeIndex =
    glm::uvec4(fieldObject.resolution, fieldObject.volumeIndex);

  gpu.config.x = primitiveCount;
  gpu.config.y = fieldRepresentation;
  gpu.config.z = fieldObject.unboundedScan;
  gpu.config.w = primitiveOffset;

  gpu.localOrigin = fieldObject.gridOrigin;

  return gpu;
}

dunya::field::Aabb gridBox(
  std::span<const dunya::field::Primitive> primitives
) {
  const std::optional<dunya::field::Aabb> extent =
    dunya::field::boundedExtent(primitives);

  dunya::field::Aabb boundedExtentBox =
    extent.value_or(dunya::field::Aabb{glm::vec3(0.0f), glm::vec3(1.0f)});

  const glm::vec3 margin(FIELD_GRID_MARGIN);

  return {boundedExtentBox.minimum - margin, boundedExtentBox.maximum + margin};
}

void refreshDerived(
  FieldObject& fieldObject,
  std::span<const dunya::field::Primitive> primitives
) {
  fieldObject.unboundedScan = 0;

  for (size_t i = 0; i < primitives.size(); ++i) {
    if (primitives[i].bounds.w <= 0.0f) {
      fieldObject.unboundedScan = static_cast<uint32_t>(i) + 1u;
    }
  }

  const dunya::field::Aabb box = gridBox(primitives);

  fieldObject.gridOrigin = glm::vec4(box.minimum, 1.0f);

  fieldObject.voxelSize =
    dunya::field::voxelSize(box.minimum, box.maximum, fieldObject.resolution);
}
