#pragma once

#include <dunya/field/analytic/analytic.h>
#include <dunya/field/field.h>

#include <glm/glm.hpp>

#include <span>

namespace dunya::objectmodel {

// The lattice a field is sampled onto. origin keeps its vec4 shape because
// FieldRecord copies it wholesale before overwriting .w.
struct FieldGrid {
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
  FieldGrid& grid,
  std::span<const dunya::field::Primitive> primitives
);

}  // namespace dunya::objectmodel
