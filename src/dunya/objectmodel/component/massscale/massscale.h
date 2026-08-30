#pragma once

#include <dunya/objectmodel/trait/authored/authored.h>

#include <dunya/objectmodel/trait/selfcontained/selfcontained.h>

namespace dunya::objectmodel {

struct MassScale {
  float factor = 1.0f;
};

template<>
inline constexpr bool selfContained<MassScale> = true;

template<>
inline constexpr bool authored<MassScale> = true;

}
