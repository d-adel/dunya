#pragma once

#include <dunya/objectmodel/authored/authored.h>

namespace dunya::objectmodel {

struct StaticBody {};

template<>
inline constexpr bool authored<StaticBody> = true;

}
