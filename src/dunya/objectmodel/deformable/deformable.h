#pragma once

#include <dunya/objectmodel/authored/authored.h>

#include <dunya/objectmodel/selfcontained/selfcontained.h>

namespace dunya::objectmodel {

struct Deformable {};

template<>
inline constexpr bool selfContained<Deformable> = true;

template<>
inline constexpr bool authored<Deformable> = true;

}
