#pragma once

#include <dunya/objectmodel/selfcontained/selfcontained.h>

namespace dunya::objectmodel {

struct Deformable {};

template<>
inline constexpr bool selfContained<Deformable> = true;

}
