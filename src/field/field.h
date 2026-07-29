#pragma once
#include "glm/glm.hpp"

/* Shape parameter conventions
 * Sphere: (radius, -)
 * Box: (halfExtents.xyz -)
 */

/* Config parameter conventions
 * .x = Shape type (0 = sphere, 1 = box, 2 = plane..)
 * .y = material id
 */

struct FieldPushConstants {
  glm::mat4 inverseViewProj;
  glm::vec4 cameraPos;
};

struct Primitive {
  glm::mat4 inverseModel;
  glm::vec4 shape;
  glm::uvec4 config;
};
