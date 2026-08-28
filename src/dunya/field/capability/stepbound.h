#pragma once

#include <glm/glm.hpp>

#include <concepts>

namespace dunya::field {

// How far a ray may travel along direction from point without crossing the
// field's zero surface. The direction is there so an implementation can clamp
// against whatever region its bound was computed for.
template<typename T>
concept StepBounded =
  requires(const T& field, const glm::vec3& point, const glm::vec3& direction) {
    { stepBound(field, point, direction) } -> std::convertible_to<float>;
  };

}  // namespace dunya::field
