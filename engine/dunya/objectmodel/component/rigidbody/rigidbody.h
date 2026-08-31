#pragma once

#include <dunya/objectmodel/trait/transient/transient.h>

#include <cstdint>

namespace dunya::objectmodel {

struct RigidBody {
  uint32_t id = UINT32_MAX;
};

template<>
inline constexpr bool transient<RigidBody> = true;

}
