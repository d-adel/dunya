#include "runtime.ih"

namespace dunya::runtime {

// The JoltLibrary reference is a lifetime requirement rather than data:
// PhysicsWorld allocates through the pointer RegisterDefaultAllocator installs.
Runtime::Runtime(const objectmodel::World& source, physics::JoltLibrary&) {
  objectmodel::instantiateWorld(source, m_world);
}

objectmodel::World& Runtime::world() noexcept {
  return m_world;
}

const objectmodel::World& Runtime::world() const noexcept {
  return m_world;
}

physics::PhysicsWorld& Runtime::physics() noexcept {
  return m_physicsWorld;
}

}  // namespace dunya::runtime
