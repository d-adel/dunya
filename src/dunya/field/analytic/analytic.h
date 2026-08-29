#pragma once

#include <dunya/field/capability/distancefield.h>
#include <dunya/field/capability/gradientquery.h>
#include <dunya/field/capability/materialquery.h>
#include <dunya/field/capability/stepbound.h>
#include <dunya/field/field.h>

#include <glm/glm.hpp>

#include <cstdint>
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

// The fold produces both in one walk, so the bake gets a material per lattice
// point without a second pass over the primitives.
struct AnalyticSample {
  float distance = 0.0f;
  uint32_t material = 0;
};

// One step of the fold sample() runs: an accumulated value meeting one more
// primitive, under that primitive's own operation. Exposed because writing a
// primitive into a stored lattice is the same operation applied to a grid
// rather than to a walk, and two spellings of a CSG operator is the drift
// idiom 27 is about.
AnalyticSample combine(
  const AnalyticSample& accumulated,
  const Primitive& primitive,
  const glm::vec3& point
);

AnalyticSample sample(
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

// Borrowed, never owning. It exists so the analytic representation has a name
// the capabilities can attach to; a bare span cannot carry that meaning.
struct AnalyticFieldView {
  std::span<const Primitive> primitives;
};

float distance(AnalyticFieldView field, const glm::vec3& point);

uint32_t material(AnalyticFieldView field, const glm::vec3& point);

glm::vec3 gradient(
  AnalyticFieldView field,
  const glm::vec3& point,
  float epsilon = DEFAULT_GRADIENT_EPSILON
);

// The value itself, because every operation here keeps the field 1-Lipschitz,
// so no zero of the field is nearer than the value at this point.
float stepBound(
  AnalyticFieldView field,
  const glm::vec3& point,
  const glm::vec3& direction
);

static_assert(DistanceField<AnalyticFieldView>);
static_assert(GradientQueryable<AnalyticFieldView>);
static_assert(MaterialQueryable<AnalyticFieldView>);
static_assert(StepBounded<AnalyticFieldView>);

}  // namespace dunya::field
