#pragma once

#include <dunya/objectmodel/selfcontained/selfcontained.h>

namespace dunya::objectmodel {

struct Deformed {};

template<>
inline constexpr bool selfContained<Deformed> = true;

}  // namespace dunya::objectmodel
