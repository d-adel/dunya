#pragma once

#include <dunya/objectmodel/component/pose/pose.h>
#include <dunya/objectmodel/trait/selfcontained/selfcontained.h>

namespace dunya::objectmodel {

struct RenderPose {
  Pose pose;
};

template<>
inline constexpr bool selfContained<RenderPose> = true;

}
