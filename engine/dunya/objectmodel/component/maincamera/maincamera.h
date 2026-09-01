#pragma once

#include <dunya/objectmodel/trait/authored/authored.h>

namespace dunya::objectmodel {

struct MainCamera {};

template<>
inline constexpr bool authored<MainCamera> = true;

}
