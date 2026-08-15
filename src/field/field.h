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

/* bounds is a world-space bounding sphere, xyz centre and w radius, used to
 * skip primitives that cannot affect a sample. **A radius of zero means "no
 * bound known" and the primitive is never skipped**, so a default-constructed
 * Primitive is slow rather than wrong. Planes are unbounded and stay at zero.
 */

struct Primitive {
  glm::mat4 inverseModel;
  glm::vec4 shape;
  glm::uvec4 shapeConfig;
  glm::vec4 bounds;
};

static_assert(
  sizeof(Primitive) == 112,
  "Primitive must keep the layout the field shader indexes by"
);

struct FieldSample {
  float distance = 0.0f;
  uint32_t material = 0;
};

}  // namespace dunya::field
