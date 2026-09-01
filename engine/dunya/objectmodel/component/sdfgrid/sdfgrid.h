#pragma once

#include <dunya/core/config/config.h>
#include <dunya/objectmodel/trait/authored/authored.h>

#include <dunya/field/analytic/analytic.h>
#include <dunya/field/field.h>

#include <glm/glm.hpp>

#include <optional>
#include <span>

namespace dunya::objectmodel {

struct SdfGrid {
  glm::vec3 voxelSize{1.0f};
  glm::uvec3 resolution{0u};
  glm::vec4 origin{0.0f};
  std::optional<float> margin{};
  std::optional<float> shadowCullMargin{};
};

[[nodiscard]] float gridMargin(
  const SdfGrid& grid,
  std::span<const dunya::field::Primitive> primitives
);

[[nodiscard]] float fittedMargin(
  std::span<const dunya::field::Primitive> primitives,
  const glm::uvec3& resolution
);

[[nodiscard]] float shadowCullMarginOf(
  const SdfGrid& grid,
  std::span<const dunya::field::Primitive> primitives
);

dunya::field::Aabb gridBox(
  std::span<const dunya::field::Primitive> primitives,
  float margin = dunya::core::FIELD_GRID_MARGIN
);

[[nodiscard]] dunya::field::Aabb casterBox(
  std::span<const dunya::field::Primitive> primitives,
  const SdfGrid& grid
);

void fitToPrimitives(
  SdfGrid& grid,
  std::span<const dunya::field::Primitive> primitives
);

template<>
inline constexpr bool authored<SdfGrid> = true;

}
