#pragma once

#include <dunya/field/field.h>
#include <dunya/field/analytic/analytic.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include <cstdint>

namespace dunya::objectmodel {

struct FieldObject {
  glm::vec3 position{0.0f};
  glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
  glm::vec3 voxelSize{1.0f};
  glm::uvec3 resolution{0u};

  uint32_t volumeIndex = UINT32_MAX;

  bool dirty = true;

  glm::vec4 gridOrigin{0.0f};

  void rotate(float radiansPerSecond, const glm::vec3& axis, float dt) {
    float angle = radiansPerSecond * dt;

    glm::quat deltaRotation = glm::angleAxis(angle, glm::normalize(axis));

    rotation = glm::normalize(deltaRotation * rotation);
  }

  glm::mat4 model() const {
    glm::mat4 rotationMatrix = glm::mat4_cast(rotation);

    glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), position);

    return translationMatrix * rotationMatrix;
  }
};

dunya::field::Aabb gridBox(std::span<const dunya::field::Primitive> primitives);

void refreshDerived(
  FieldObject& fieldObject,
  std::span<const dunya::field::Primitive> primitives
);

}  // namespace dunya::objectmodel
