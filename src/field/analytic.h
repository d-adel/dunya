#pragma once

#include "field/field.h"

#include "glm/glm.hpp"

#include <span>

namespace dunya::field {

// Matches normalSampleOffset in field-shader.frag, so a CPU query and the
// drawn image agree. Tests tighten it; physics may want it tighter still,
// since contact normals are this gradient and a coarse offset reads as jitter.
constexpr float DEFAULT_GRADIENT_EPSILON = 0.01f;

FieldSample sample(
  std::span<const Primitive> primitives,
  const glm::vec3& point
);

// The true gradient, not normalised: away from surfaces its magnitude is 1,
// which is the property that says the field is a distance field.
glm::vec3 gradient(
  std::span<const Primitive> primitives,
  const glm::vec3& point,
  float epsilon = DEFAULT_GRADIENT_EPSILON
);

glm::vec3 normal(
  std::span<const Primitive> primitives,
  const glm::vec3& point,
  float epsilon = DEFAULT_GRADIENT_EPSILON
);

}  // namespace dunya::field
