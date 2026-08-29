#pragma once

#include <dunya/objectmodel/selfcontained/selfcontained.h>

namespace dunya::objectmodel {

// How much heavier this body is than the field it is built on says it should
// be. A factor rather than a weight, because mass following the geometry is
// the property a volume representation is for: a carved body has to get
// lighter, and what a caller meant by "make this 150 kg" was the material, not
// a number that survives losing half of itself.
//
// Absent on almost everything. Presence is the state; there is no factor of
// one to distinguish from a body nobody has asked about.
struct MassScale {
  float factor = 1.0f;
};

// Its own bytes and nothing else: no arena range, no pool slot, nothing
// derived from another component.
template<>
inline constexpr bool selfContained<MassScale> = true;

}  // namespace dunya::objectmodel
