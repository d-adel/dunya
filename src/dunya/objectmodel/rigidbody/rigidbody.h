#pragma once

#include <cstdint>

namespace dunya::objectmodel {

// This entity is simulated by a body in the runtime's PhysicsWorld. The body
// lives in dunya::physics; only the number crosses, which keeps this library
// free of Jolt.
struct RigidBody {
  uint32_t id = UINT32_MAX;
};

// Deliberately not SelfContained: the id names a body that has to be created
// and destroyed alongside it, so writing one is never just writing bytes.

}  // namespace dunya::objectmodel
