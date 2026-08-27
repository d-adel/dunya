#include "fieldobject.ih"

namespace dunya::objectmodel {

FieldObjectGPU fromFieldObject(
  const FieldObject& fieldObject,
  uint32_t primitiveOffset,
  uint32_t primitiveCount,
  uint32_t fieldRepresentation
) {
  FieldObjectGPU gpu{};

  gpu.model = fieldObject.model();
  gpu.inverseModel = glm::inverse(gpu.model);

  gpu.voxelSize =
    glm::vec4(fieldObject.voxelSize, dunya::core::FIELD_GRID_MARGIN);

  gpu.resolutionVolumeIndex =
    glm::uvec4(fieldObject.resolution, fieldObject.volumeIndex);

  gpu.config.x = primitiveCount;
  gpu.config.y = fieldRepresentation;
  gpu.config.z = 1u;
  gpu.config.w = primitiveOffset;

  gpu.localOrigin = fieldObject.gridOrigin;

  // .w carried a homogeneous 1 that nothing read. It now says where this
  // object's brick bounds start, so one volume index still addresses every
  // sampled resource it owns.
  gpu.localOrigin.w =
    fieldObject.volumeIndex == UINT32_MAX
      ? 0.0f
      : static_cast<float>(
          fieldObject.volumeIndex * dunya::core::MAX_BRICKS_PER_OBJECT
        );

  return gpu;
}

dunya::field::Aabb gridBox(
  std::span<const dunya::field::Primitive> primitives
) {
  const std::optional<dunya::field::Aabb> extent =
    dunya::field::boundedExtent(primitives);

  dunya::field::Aabb boundedExtentBox =
    extent.value_or(dunya::field::Aabb{glm::vec3(0.0f), glm::vec3(1.0f)});

  const glm::vec3 margin(dunya::core::FIELD_GRID_MARGIN);

  return {boundedExtentBox.minimum - margin, boundedExtentBox.maximum + margin};
}

void refreshDerived(
  FieldObject& fieldObject,
  std::span<const dunya::field::Primitive> primitives
) {
  const dunya::field::Aabb box = gridBox(primitives);

  fieldObject.gridOrigin = glm::vec4(box.minimum, 1.0f);

  fieldObject.voxelSize =
    dunya::field::voxelSize(box.minimum, box.maximum, fieldObject.resolution);
}

}  // namespace dunya::objectmodel
