#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

namespace dunya::objectmodel {

// The one light a scene is lit by. Scene description rather than renderer
// state, which is why it sits beside Camera: the renderer reads it, the editor
// will want to move it, and physics will not care.
//
// Not a component. There is one per scene and nothing indexes it, and making
// it a component would say otherwise.
struct DirectionalLight {
  // Toward the light, not the way the light travels: it is what a dot with a
  // surface normal wants, and both shaders were already written that way.
  // Normalised on construction so a hand-written value cannot arrive otherwise.
  glm::vec3 direction{0.4f, 1.0f, 0.6f};

  // What a surface facing away still receives.
  float ambient = 0.06f;
};

// The direction as the shaders need it. Kept as a function rather than
// normalising in place, because the value a person edits should stay the value
// they typed.
inline glm::vec3 toLight(const DirectionalLight& light) {
  const float length = glm::length(light.direction);

  return length > glm::epsilon<float>() ? light.direction / length
                                        : glm::vec3(0.0f, 1.0f, 0.0f);
}

}  // namespace dunya::objectmodel
