#pragma once

#include <dunya/core/config/config.h>
#include <dunya/objectmodel/component/lens/lens.h>
#include <dunya/objectmodel/component/pose/pose.h>
#include <dunya/objectmodel/entity/entity.h>
#include <dunya/view/drawmode/drawmode.h>
#include <dunya/view/grid/gridstyle.h>

#include <cstdint>

namespace dunya::view {

using TargetId = uint32_t;
using ViewportId = uint32_t;

inline constexpr TargetId INVALID_TARGET = UINT32_MAX;
inline constexpr ViewportId INVALID_VIEWPORT = UINT32_MAX;

struct MarchSettings {
  float epsilon = DUNYA_MARCH_EPSILON;
  float maxDistance = DUNYA_MARCH_MAX_DISTANCE;
  float omega = DUNYA_MARCH_OMEGA;
  float gradientEpsilon = DUNYA_GRADIENT_EPSILON;

  float shadowMaxDistance = DUNYA_SHADOW_MAX_DISTANCE;
  float shadowSharpness = DUNYA_SHADOW_SHARPNESS;

  uint32_t maxIterations = DUNYA_MARCH_MAX_ITERATIONS;
};

struct Viewport {
  TargetId target = INVALID_TARGET;

  dunya::objectmodel::Entity camera = dunya::objectmodel::INVALID_ENTITY;

  dunya::objectmodel::Pose pose{};
  dunya::objectmodel::Lens lens{};

  DrawMode mode = DrawMode::Both;

  uint32_t fieldRepresentation = dunya::core::FIELD_SAMPLED;

  MarchSettings march{};

  float supersample = 1.0f;

  bool gridVisible = false;
  GridPlane gridPlane{};
  GridStyle gridStyle{};
};

}
