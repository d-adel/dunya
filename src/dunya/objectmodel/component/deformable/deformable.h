#pragma once

#include <dunya/objectmodel/trait/authored/authored.h>

#include <dunya/objectmodel/trait/selfcontained/selfcontained.h>

namespace dunya::objectmodel {

struct Deformable {};

template<>
inline constexpr bool selfContained<Deformable> = true;

template<>
inline constexpr bool authored<Deformable> = true;

}
