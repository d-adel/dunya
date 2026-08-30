#pragma once

#include <dunya/objectmodel/selfcontained/selfcontained.h>

#include <cstdint>

namespace dunya::objectmodel {

struct Mesh {
  uint32_t index = UINT32_MAX;
};

template<>
inline constexpr bool selfContained<Mesh> = true;

}  // namespace dunya::objectmodel
