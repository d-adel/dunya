#pragma once

#include <glm/glm.hpp>

#include <concepts>

namespace dunya::field {

// A query, not a kind of field: a density field can answer it too, which is why
// the name says queryable rather than GradientField.
template<typename T>
concept GradientQueryable = requires(const T& field, const glm::vec3& point) {
  { gradient(field, point) } -> std::convertible_to<glm::vec3>;
};

}  // namespace dunya::field
