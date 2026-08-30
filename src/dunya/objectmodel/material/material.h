#pragma once

#include <dunya/objectmodel/selfcontained/selfcontained.h>

#include <cstdint>

namespace dunya::objectmodel {

struct Material {
  uint32_t index = UINT32_MAX;
};

template<>
inline constexpr bool selfContained<Material> = true;

}
