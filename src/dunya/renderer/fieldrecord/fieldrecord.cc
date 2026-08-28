#include "fieldrecord.ih"

namespace dunya::renderer {

FieldRecord makeFieldRecord(
  const dunya::objectmodel::FieldObject& fieldObject,
  uint32_t primitiveOffset,
  uint32_t primitiveCount,
  uint32_t fieldRepresentation
) {
  FieldRecord record{};

  record.model = fieldObject.model();
  record.inverseModel = glm::inverse(record.model);

  record.voxelSize =
    glm::vec4(fieldObject.voxelSize, dunya::core::FIELD_GRID_MARGIN);

  record.resolutionVolumeIndex =
    glm::uvec4(fieldObject.resolution, fieldObject.volumeIndex);

  record.config.x = primitiveCount;
  record.config.y = fieldRepresentation;
  record.config.z = 1u;
  record.config.w = primitiveOffset;

  record.localOrigin = fieldObject.gridOrigin;

  // .w carried a homogeneous 1 that nothing read. It now says where this
  // object's brick bounds start, so one volume index still addresses every
  // sampled resource it owns.
  record.localOrigin.w =
    fieldObject.volumeIndex == UINT32_MAX
      ? 0.0f
      : static_cast<float>(
          fieldObject.volumeIndex * dunya::core::BRICK_TABLE_STRIDE
        );

  return record;
}

}  // namespace dunya::renderer
