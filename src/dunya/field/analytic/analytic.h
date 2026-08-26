#pragma once

#include <dunya/field/field.h>

#include <glm/glm.hpp>

#include <optional>
#include <span>

namespace dunya::field {

// The shaders are compiled with this same value, so a CPU query and the drawn
// image agree. Tests tighten it; physics may want it tighter still, since
// contact normals are this gradient and a coarse offset reads as jitter.
constexpr float DEFAULT_GRADIENT_EPSILON = DUNYA_GRADIENT_EPSILON;

// Fills in the bounding sphere the evaluator culls against. Must be called
// after inverseModel, shape and shapeConfig are set; leaving it uncalled costs
// performance, never correctness.
void updateBounds(Primitive& primitive);

struct Aabb {
  glm::vec3 minimum;
  glm::vec3 maximum;
};

// The box enclosing every primitive that has a bound. Unbounded ones - planes -
// contribute nothing, which is precisely why they stay analytic: no finite grid
// can hold them.
std::optional<Aabb> boundedExtent(std::span<const Primitive> primitives);

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
