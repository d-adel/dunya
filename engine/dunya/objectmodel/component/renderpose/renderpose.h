#pragma once

#include <dunya/objectmodel/trait/transient/transient.h>

#include <dunya/objectmodel/component/pose/pose.h>
#include <dunya/objectmodel/entity/entity.h>
#include <dunya/objectmodel/trait/selfcontained/selfcontained.h>

#include <entt/entt.hpp>

namespace dunya::objectmodel {

struct RenderPose {
  Pose pose;
};

template<>
inline constexpr bool selfContained<RenderPose> = true;

[[nodiscard]] const Pose& drawnPose(
  const entt::registry& registry,
  Entity entity
);

template<>
inline constexpr bool transient<RenderPose> = true;

}
