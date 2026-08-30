#pragma once

#include <dunya/objectmodel/trait/selfcontained/selfcontained.h>

namespace dunya::objectmodel {

struct Deformed {};

template<>
inline constexpr bool selfContained<Deformed> = true;

}
