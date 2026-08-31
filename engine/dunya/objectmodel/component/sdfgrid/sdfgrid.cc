#include "sdfgrid.ih"

namespace dunya::objectmodel {

dunya::field::Aabb gridBox(
  std::span<const dunya::field::Primitive> primitives,
  float margin
) {
  const std::optional<dunya::field::Aabb> extent =
    dunya::field::boundedExtent(primitives);

  dunya::field::Aabb boundedExtentBox =
    extent.value_or(dunya::field::Aabb{glm::vec3(0.0f), glm::vec3(1.0f)});

  const glm::vec3 pad(margin);

  return {boundedExtentBox.minimum - pad, boundedExtentBox.maximum + pad};
}

void fitToPrimitives(
  SdfGrid& grid,
  std::span<const dunya::field::Primitive> primitives
) {
  const dunya::field::Aabb box = gridBox(primitives, grid.margin);

  grid.origin = glm::vec4(box.minimum, 1.0f);

  grid.voxelSize =
    dunya::field::voxelSize(box.minimum, box.maximum, grid.resolution);
}

}
