#pragma once

#include <dunya/objectmodel/selfcontained/selfcontained.h>

#include <cstdint>

namespace dunya::objectmodel {

// This entity is drawn as a loaded mesh. The buffers live in dunya::renderer;
// only the number crosses, which keeps this library free of Vulkan.
struct Mesh {
  uint32_t index = UINT32_MAX;
};

template<>
inline constexpr bool selfContained<Mesh> = true;

}  // namespace dunya::objectmodel
