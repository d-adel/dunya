#pragma once

#include <glm/glm.hpp>

#include <concepts>

namespace dunya::field {

// A different kind of scalar, not a weaker distance: there is no surface and no
// sign. Declared ahead of M22; no representation satisfies it yet.
template<typename T>
concept DensityField = requires(const T& field, const glm::vec3& point) {
  { density(field, point) } -> std::convertible_to<float>;
};

}  // namespace dunya::field
