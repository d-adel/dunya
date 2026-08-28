#pragma once

#include <dunya/objectmodel/selfcontained/selfcontained.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace dunya::objectmodel {

// Where an entity is. No scale: a field entity cannot take a non-uniform one
// without breaking the distance property.
struct Pose {
  glm::vec3 position{0.0f};
  glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
};

template<>
inline constexpr bool selfContained<Pose> = true;

// A free function rather than a member: components are data, and step 9 hands
// them to EnTT's snapshot.
inline glm::mat4 model(const Pose& pose) {
  glm::mat4 rotationMatrix = glm::mat4_cast(pose.rotation);

  glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), pose.position);

  return translationMatrix * rotationMatrix;
}

}  // namespace dunya::objectmodel
