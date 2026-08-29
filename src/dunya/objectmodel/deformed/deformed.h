#pragma once

#include <dunya/objectmodel/selfcontained/selfcontained.h>

namespace dunya::objectmodel {

// The lattice has been written in place, so it is no longer the bake of the
// primitives it started from. Deformable says it may happen; this says it has.
// Absence is the state, and what reads it is anything that would otherwise
// treat two objects with equal primitives as interchangeable.
struct Deformed {};

template<>
inline constexpr bool selfContained<Deformed> = true;

}  // namespace dunya::objectmodel
