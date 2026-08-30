#pragma once

#include <dunya/objectmodel/selfcontained/selfcontained.h>

namespace dunya::objectmodel {

struct MassScale {
  float factor = 1.0f;
};

template<>
inline constexpr bool selfContained<MassScale> = true;

}  // namespace dunya::objectmodel
