#pragma once

#include <dunya/field/analytic/analytic.h>
#include <dunya/field/field.h>

#include <glm/glm.hpp>

#include <span>

namespace dunya::objectmodel {

struct SdfGrid {
  glm::vec3 voxelSize{1.0f};
  glm::uvec3 resolution{0u};
  glm::vec4 origin{0.0f};
};

dunya::field::Aabb gridBox(std::span<const dunya::field::Primitive> primitives);

void fitToPrimitives(
  SdfGrid& grid,
  std::span<const dunya::field::Primitive> primitives
);

}
