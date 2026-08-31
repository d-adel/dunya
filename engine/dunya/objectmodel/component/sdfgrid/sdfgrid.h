#pragma once

#include <dunya/core/config/config.h>
#include <dunya/objectmodel/trait/authored/authored.h>

#include <dunya/field/analytic/analytic.h>
#include <dunya/field/field.h>

#include <glm/glm.hpp>

#include <span>

namespace dunya::objectmodel {

struct SdfGrid {
  glm::vec3 voxelSize{1.0f};
  glm::uvec3 resolution{0u};
  glm::vec4 origin{0.0f};
  float margin{dunya::core::FIELD_GRID_MARGIN};
};

dunya::field::Aabb gridBox(
  std::span<const dunya::field::Primitive> primitives,
  float margin = dunya::core::FIELD_GRID_MARGIN
);

void fitToPrimitives(
  SdfGrid& grid,
  std::span<const dunya::field::Primitive> primitives
);

template<>
inline constexpr bool authored<SdfGrid> = true;

}
