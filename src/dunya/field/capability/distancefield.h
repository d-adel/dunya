#pragma once

#include <glm/glm.hpp>

#include <concepts>

namespace dunya::field {

// Zero is the surface and the sign says inside. The magnitude is distance-like
// and is not promised to be the exact Euclidean distance.
template <typename T>
concept DistanceField = requires(const T& field, const glm::vec3& point) {
  { distance(field, point) } -> std::convertible_to<float>;
};

}  // namespace dunya::field
