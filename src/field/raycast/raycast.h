#pragma once

#include "field/field.h"
#include "field/analytic/analytic.h"

#include "glm/glm.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <utility>

namespace dunya::field {

// The shaders are compiled with these same values. A CPU query that stops at a
// different epsilon than the shader marched to reports a surface the image does
// not show.
constexpr float MARCH_EPSILON = DUNYA_MARCH_EPSILON;
constexpr int MARCH_MAX_ITERATIONS = DUNYA_MARCH_MAX_ITERATIONS;
constexpr float MARCH_MAX_DISTANCE = DUNYA_MARCH_MAX_DISTANCE;
constexpr float MARCH_OMEGA = DUNYA_MARCH_OMEGA;
constexpr float RAY_EPSILON = 1e-8f;

struct Ray {
  glm::vec3 origin;
  glm::vec3 direction;
};

struct RayHit {
  glm::vec3 position;
  float travelled = 0.0f;
  uint32_t material = 0;
};

// omega is the over-relaxation multiplier. 1.0 is plain sphere tracing; above
// 1 each step overshoots deliberately and backs off when consecutive unbounding
// spheres come apart, which is what keeps it safe under a loose bound.
struct MarchSettings {
  float epsilon = MARCH_EPSILON;
  int maxIterations = MARCH_MAX_ITERATIONS;
  float maxDistance = MARCH_MAX_DISTANCE;
  float omega = MARCH_OMEGA;
};

// Mirrors the reconstruction in field-shader.frag: a point on the far plane,
// unprojected and divided through, gives the direction from the eye.
Ray screenPointToRay(
  const glm::mat4& inverseViewProj,
  const glm::vec3& cameraPosition,
  const glm::vec2& ndc
);

std::optional<RayHit> raymarch(
  std::span<const Primitive> primitives,
  const Ray& ray,
  const MarchSettings& settings = {}
);

std::optional<std::pair<float, float>> intersect(
  const Aabb& box,
  const Ray& ray
);

}  // namespace dunya::field
