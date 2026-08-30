#pragma once

#include <dunya/objectmodel/pose/pose.h>
#include <dunya/objectmodel/selfcontained/selfcontained.h>

namespace dunya::objectmodel {

struct RenderPose {
  Pose pose;
};

template<>
inline constexpr bool selfContained<RenderPose> = true;

}
