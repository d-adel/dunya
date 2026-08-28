#include "runtime.ih"

namespace dunya::runtime {

// Neither parameter is read yet; the names are omitted to say so. The
// JoltLibrary reference is a lifetime requirement: PhysicsWorld allocates
// through the pointer RegisterDefaultAllocator installs.
Runtime::Runtime(const objectmodel::World&, physics::JoltLibrary&) {}

}  // namespace dunya::runtime
