#pragma once

#include <dunya/field/analytic/analytic.h>
#include <dunya/field/field.h>

#include <glm/glm.hpp>

#include <span>

namespace dunya::objectmodel {

// A lattice holding signed distance - the sampled half of the distance row.
// DensityGrid is its sibling at M22; the meaning is the type, not a tag.
struct SdfGrid {
  glm::vec3 voxelSize{1.0f};
  glm::uvec3 resolution{0u};
  glm::vec4 origin{0.0f};
};

// The box a grid has to cover: the primitives' bounded extent, plus the margin
// on every side.
dunya::field::Aabb gridBox(std::span<const dunya::field::Primitive> primitives);

// Re-fit the lattice to the primitives it samples. Resolution is authored and
// stays put; origin and voxelSize are derived, so this is the only thing
// allowed to write them.
void fitToPrimitives(
  SdfGrid& grid,
  std::span<const dunya::field::Primitive> primitives
);

}  // namespace dunya::objectmodel
