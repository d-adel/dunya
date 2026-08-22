#include "fieldobject.ih"

void makeShared(
  uint32_t fieldRepresentation,
  const std::vector<FieldObject>& fieldObjects,
  std::vector<FieldObjectShared>& out
) {
  out.clear();
  out.reserve(fieldObjects.size());

  for (const FieldObject& fieldObject : fieldObjects) {
    FieldObjectShared shared{};

    glm::mat4 rotationMatrix = glm::mat4_cast(fieldObject.rotation);

    glm::mat4 translationMatrix =
      glm::translate(glm::mat4(1.0f), fieldObject.position);

    shared.model = translationMatrix * rotationMatrix;
    shared.inverseModel = glm::inverse(shared.model);
    // w is the slack baked around the contents: the bound returned outside the
    // grid must never be small enough to read as arrival.
    shared.voxelSize = glm::vec4(fieldObject.voxelSize, FIELD_GRID_MARGIN);
    shared.resolutionVolumeIndex =
      glm::uvec4(fieldObject.resolution, fieldObject.volumeIndex);

    shared.config.x = static_cast<glm::uint>(fieldObject.editList.size());
    shared.config.y = fieldRepresentation;
    shared.config.z = fieldObject.unboundedScan;

    shared.localOrigin = shared.inverseModel * fieldObject.gridOrigin;

    out.push_back(shared);
  }
}

dunya::field::Aabb gridBox(const FieldObject& fieldObject) {
  const std::optional<dunya::field::Aabb> extent =
    dunya::field::boundedExtent(fieldObject.editList);

  dunya::field::Aabb boundedExtentBox =
    extent.value_or(dunya::field::Aabb{glm::vec3(0.0f), glm::vec3(1.0f)});

  const glm::vec3 margin(FIELD_GRID_MARGIN);

  return {boundedExtentBox.minimum - margin, boundedExtentBox.maximum + margin};
}

void refreshDerived(FieldObject& fieldObject) {
  // One past the last unbounded primitive, not a count: the fold outside the
  // grid must keep the array's order or the CSG changes.
  fieldObject.unboundedScan = 0;

  for (size_t i = 0; i < fieldObject.editList.size(); ++i) {
    if (fieldObject.editList[i].bounds.w <= 0.0f) {
      fieldObject.unboundedScan = static_cast<uint32_t>(i) + 1u;
    }
  }

  // The grid follows what it holds, or new geometry lands off the lattice.
  const dunya::field::Aabb box = gridBox(fieldObject);

  fieldObject.position = (box.minimum + box.maximum) * 0.5f;
  fieldObject.gridOrigin = glm::vec4(box.minimum, 1.0f);

  fieldObject.voxelSize =
    dunya::field::voxelSize(box.minimum, box.maximum, fieldObject.resolution);
}
