#include "fieldrecord.ih"

namespace dunya::renderer {

FieldRecord makeFieldRecord(
  const dunya::objectmodel::Pose& pose,
  const dunya::objectmodel::FieldGrid& grid,
  const dunya::objectmodel::BakedVolume& volume,
  uint32_t primitiveOffset,
  uint32_t primitiveCount,
  uint32_t fieldRepresentation
) {
  FieldRecord record{};

  record.model = dunya::objectmodel::model(pose);
  record.inverseModel = glm::inverse(record.model);

  record.voxelSize = glm::vec4(grid.voxelSize, dunya::core::FIELD_GRID_MARGIN);

  record.resolutionVolumeIndex = glm::uvec4(grid.resolution, volume.index);

  record.config.x = primitiveCount;
  record.config.y = fieldRepresentation;
  record.config.z = 1u;
  record.config.w = primitiveOffset;

  record.localOrigin = grid.origin;

  // .w carried a homogeneous 1 that nothing read. It now says where this
  // object's brick bounds start, so one volume index still addresses every
  // sampled resource it owns.
  record.localOrigin.w =
    volume.index == UINT32_MAX
      ? 0.0f
      : static_cast<float>(volume.index * dunya::core::BRICK_TABLE_STRIDE);

  return record;
}

}  // namespace dunya::renderer
