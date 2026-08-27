#pragma once

#include <glm/glm.hpp>

#include <concepts>
#include <cstdint>

namespace dunya::field {

// Discrete metadata attached to geometry rather than a scalar field: it is
// selected, never interpolated, since filtering an id yields material 3.7.
template <typename T>
concept MaterialQueryable = requires(const T& field, const glm::vec3& point) {
  { material(field, point) } -> std::convertible_to<uint32_t>;
};

}  // namespace dunya::field
