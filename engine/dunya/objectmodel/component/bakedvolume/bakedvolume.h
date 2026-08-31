#pragma once

#include <dunya/objectmodel/trait/transient/transient.h>

#include <cstdint>

namespace dunya::objectmodel {

struct BakedVolume {
  uint32_t index = UINT32_MAX;
};

template<>
inline constexpr bool transient<BakedVolume> = true;

}
