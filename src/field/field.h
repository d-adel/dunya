#pragma once
#include "glm/glm.hpp"

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

/* Config parameter marching
 * .x = epsilon
 * .y = maxIter
 * .z = sampleOffset
 * .w = bias
 */

struct FieldFrame {
  glm::uvec4 primitiveCount;
};

struct Primitive {
  glm::mat4 inverseModel;
  glm::vec4 shape;
  glm::uvec4 shapeConfig;
};
