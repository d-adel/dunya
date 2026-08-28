#pragma once

#include <cstdint>

namespace dunya::objectmodel {

// This entity owns a slot in the volume pool. The pool lives in
// dunya::renderer; only the number crosses, keeping this library Vulkan-free.
struct BakedVolume {
  uint32_t index = UINT32_MAX;
};

}  // namespace dunya::objectmodel
