#pragma once

#include <glm/glm.hpp>

#include <concepts>

namespace dunya::field {

template<typename T>
concept DensityField = requires(const T& field, const glm::vec3& point) {
  { density(field, point) } -> std::convertible_to<float>;
};

}  // namespace dunya::field
