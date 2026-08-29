#pragma once

#include <dunya/core/config/config.h>
#include <dunya/field/analytic/analytic.h>
#include <dunya/field/field.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cstdint>

// Primitives every field test needs and none of them should be spelling out:
// four lines of shape indices and a bounds refresh, wrong in a way that reads
// as a physics result rather than as a typo.
namespace fixture {

inline constexpr uint32_t SPHERE_KIND = 0u;
inline constexpr uint32_t BOX_KIND = 1u;

inline dunya::field::Primitive sphere(
  const glm::vec3& centre,
  float radius,
  uint32_t operation = dunya::core::FIELD_OP_UNION
) {
  dunya::field::Primitive primitive{};

  primitive.inverseModel =
    glm::inverse(glm::translate(glm::mat4(1.0f), centre));
  primitive.shape = glm::vec4(radius, 0.0f, 0.0f, 0.0f);
  primitive.shapeConfig = glm::uvec4(SPHERE_KIND, 1u, operation, 0u);

  dunya::field::updateBounds(primitive);

  return primitive;
}

inline dunya::field::Primitive box(
  const glm::vec3& centre,
  const glm::vec3& half,
  uint32_t operation = dunya::core::FIELD_OP_UNION
) {
  dunya::field::Primitive primitive{};

  primitive.inverseModel =
    glm::inverse(glm::translate(glm::mat4(1.0f), centre));
  primitive.shape = glm::vec4(half, 0.0f);
  primitive.shapeConfig = glm::uvec4(BOX_KIND, 1u, operation, 0u);

  dunya::field::updateBounds(primitive);

  return primitive;
}

}  // namespace fixture
