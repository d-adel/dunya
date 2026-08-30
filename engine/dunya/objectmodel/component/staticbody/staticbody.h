#pragma once

#include <dunya/objectmodel/trait/authored/authored.h>

namespace dunya::objectmodel {

struct StaticBody {};

template<>
inline constexpr bool authored<StaticBody> = true;

}
