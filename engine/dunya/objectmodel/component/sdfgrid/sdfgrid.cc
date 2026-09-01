#include "sdfgrid.ih"

namespace dunya::objectmodel {

namespace {

float coarsestAxis(const glm::vec3& solid, const glm::uvec3& resolution) {
  float coarsest = 0.0f;

  for (glm::length_t axis = 0; axis != 3; ++axis) {
    if (resolution[axis] < 2u) {
      continue;
    }

    coarsest = std::max(coarsest, solid[axis] / float(resolution[axis] - 1u));
  }

  return coarsest;
}

glm::length_t coarsestIndex(
  const glm::vec3& solid,
  const glm::uvec3& resolution
) {
  glm::length_t found = 0;
  float coarsest = -1.0f;

  for (glm::length_t axis = 0; axis != 3; ++axis) {
    if (resolution[axis] < 2u) {
      continue;
    }

    const float step = solid[axis] / float(resolution[axis] - 1u);

    if (step > coarsest) {
      coarsest = step;
      found = axis;
    }
  }

  return found;
}

}

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

float fittedMargin(
  std::span<const dunya::field::Primitive> primitives,
  const glm::uvec3& resolution
) {
  const std::optional<dunya::field::Aabb> extent =
    dunya::field::boundedExtent(primitives);

  if (!extent.has_value()) {
    return dunya::core::FIELD_GRID_MARGIN;
  }

  const glm::vec3 solid = extent->maximum - extent->minimum;

  if (coarsestAxis(solid, resolution) <= 0.0f) {
    return dunya::core::FIELD_GRID_MARGIN;
  }

  const glm::length_t axis = coarsestIndex(solid, resolution);

  const float cells = float(dunya::core::FIELD_GRID_MARGIN_CELLS);

  const float room = float(resolution[axis] - 1u) - 2.0f * cells;

  if (room <= 0.0f) {
    return dunya::core::FIELD_GRID_MARGIN;
  }

  return cells * solid[axis] / room;
}

float gridMargin(
  const SdfGrid& grid,
  std::span<const dunya::field::Primitive> primitives
) {
  return grid.margin.value_or(fittedMargin(primitives, grid.resolution));
}

float shadowCullMarginOf(
  const SdfGrid& grid,
  std::span<const dunya::field::Primitive> primitives
) {
  return grid.shadowCullMargin.value_or(
    std::max(dunya::core::SHADOW_CULL_MARGIN, gridMargin(grid, primitives))
  );
}

dunya::field::Aabb casterBox(
  std::span<const dunya::field::Primitive> primitives,
  const SdfGrid& grid
) {
  return gridBox(primitives, shadowCullMarginOf(grid, primitives));
}

void fitToPrimitives(
  SdfGrid& grid,
  std::span<const dunya::field::Primitive> primitives
) {
  const dunya::field::Aabb box =
    gridBox(primitives, gridMargin(grid, primitives));

  grid.origin = glm::vec4(box.minimum, 1.0f);

  grid.voxelSize =
    dunya::field::voxelSize(box.minimum, box.maximum, grid.resolution);
}

}
