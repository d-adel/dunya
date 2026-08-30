#include "sdfrecord.ih"

namespace dunya::renderer {

RecordBounds makeRecordBounds(const SdfRecord& record) {
  const glm::uvec3 resolution(record.resolutionVolumeIndex);

  if (resolution.x == 0u || resolution.y == 0u || resolution.z == 0u) {
    return {glm::vec4(1.0f), glm::vec4(-1.0f)};
  }

  const glm::vec3 localMinimum(record.localOrigin);

  const glm::vec3 localMaximum =
    localMinimum
    + glm::vec3(record.voxelSize) * glm::vec3(resolution - glm::uvec3(1u));

  glm::vec3 minimum(std::numeric_limits<float>::max());
  glm::vec3 maximum(std::numeric_limits<float>::lowest());

  for (uint32_t corner = 0u; corner != 8u; ++corner) {
    const glm::vec3 at(
      (corner & 1u) != 0u ? localMaximum.x : localMinimum.x,
      (corner & 2u) != 0u ? localMaximum.y : localMinimum.y,
      (corner & 4u) != 0u ? localMaximum.z : localMinimum.z
    );

    const glm::vec3 world = glm::vec3(record.model * glm::vec4(at, 1.0f));

    minimum = glm::min(minimum, world);
    maximum = glm::max(maximum, world);
  }

  return {glm::vec4(minimum, 0.0f), glm::vec4(maximum, 0.0f)};
}

SdfRecord makeSdfRecord(
  const dunya::objectmodel::Pose& pose,
  const dunya::objectmodel::SdfGrid& grid,
  const dunya::objectmodel::BakedVolume& volume,
  uint32_t primitiveOffset,
  uint32_t primitiveCount,
  uint32_t fieldRepresentation
) {
  SdfRecord record{};

  record.model = dunya::objectmodel::model(pose);
  record.inverseModel = glm::inverse(record.model);

  record.voxelSize = glm::vec4(grid.voxelSize, dunya::core::FIELD_GRID_MARGIN);

  record.resolutionVolumeIndex = glm::uvec4(grid.resolution, volume.index);

  record.config.x = primitiveCount;
  record.config.y = fieldRepresentation;
  record.config.z = 1u;
  record.config.w = primitiveOffset;

  record.localOrigin = grid.origin;

  record.localOrigin.w =
    volume.index == UINT32_MAX
      ? 0.0f
      : static_cast<float>(volume.index * dunya::core::BRICK_TABLE_STRIDE);

  return record;
}

}
