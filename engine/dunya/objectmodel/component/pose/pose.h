#pragma once

#include <dunya/objectmodel/trait/authored/authored.h>

#include <dunya/objectmodel/trait/selfcontained/selfcontained.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace dunya::objectmodel {

struct Pose {
  glm::vec3 position{0.0f};
  glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
};

template<>
inline constexpr bool selfContained<Pose> = true;

inline glm::mat4 model(const Pose& pose) {
  glm::mat4 rotationMatrix = glm::mat4_cast(pose.rotation);

  glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), pose.position);

  return translationMatrix * rotationMatrix;
}

template<>
inline constexpr bool authored<Pose> = true;

}
