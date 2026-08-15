#pragma once
#include "glm/glm.hpp"

#include <cstdint>

namespace dunya::field {

/* Shape parameter conventions
 * Sphere: (radius, -)
 * Box: (halfExtents.xyz -)
 * .w = blend radius
 */

/* Config parameter shape conventions
 * .x = Shape type (0 = sphere, 1 = box, 2 = plane..)
 * .y = material id
 * .z = operation (0 = union, 1 = smooth union, 2 = intersection, 3 =
 * subtraction)
 */

/* Transforms are rigid. Sphere tracing and continuous collision both need the
 * returned distance to be a lower bound on the distance to the surface, and a
 * non-uniform scale in inverseModel breaks that.
 */

struct Primitive {
  glm::mat4 inverseModel;
  glm::vec4 shape;
  glm::uvec4 shapeConfig;
};

static_assert(
  sizeof(Primitive) == 96,
  "Primitive must keep the std140 layout the field shader indexes by"
);

struct FieldSample {
  float distance = 0.0f;
  uint32_t material = 0;
};

}  // namespace dunya::field
