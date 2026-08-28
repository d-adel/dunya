#pragma once

#include <dunya/objectmodel/selfcontained/selfcontained.h>

#include <cstdint>

namespace dunya::objectmodel {

// Which entry of the material table this entity is drawn with. Independent of
// what kind of geometry it has, which is why it is its own component.
struct Material {
  uint32_t index = UINT32_MAX;
};

template<>
inline constexpr bool selfContained<Material> = true;

}  // namespace dunya::objectmodel
