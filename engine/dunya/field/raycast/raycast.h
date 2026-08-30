#pragma once

#include <dunya/field/field.h>
#include <dunya/field/analytic/analytic.h>

#include <glm/glm.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <utility>

namespace dunya::field {

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

struct MarchSettings {
  float epsilon = MARCH_EPSILON;
  int maxIterations = MARCH_MAX_ITERATIONS;
  float maxDistance = MARCH_MAX_DISTANCE;
  float omega = MARCH_OMEGA;
};

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

}
