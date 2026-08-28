#pragma once

#include <cstdint>

namespace dunya::objectmodel {

// This entity owns a slot in the volume pool, holding the distance and material
// volumes its field was baked into. Persistent runtime ownership, by the rule
// that an index naming a resource gets it - unlike the entries slot, which
// addresses per-frame data and is generated while assembling the frame.
//
// The pool itself lives in dunya::renderer. Only the number crosses, which is
// what keeps this library free of Vulkan.
struct BakedVolume {
  uint32_t index = UINT32_MAX;
};

}  // namespace dunya::objectmodel
