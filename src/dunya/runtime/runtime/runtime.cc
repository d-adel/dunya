#include "runtime.ih"

namespace dunya::runtime {

/* Both parameters are unnamed because neither is read yet, and saying so in the
 * signature is more honest than a body that quietly ignores them.
 *
 * source is what step 9 instantiates from: the authored world is copied into
 * m_world, which cannot be a copy construction because World is non-copyable by
 * design. It will be a walk over source's entities.
 *
 * The JoltLibrary reference is a lifetime requirement rather than a value.
 * PhysicsWorld's first member is a JPH::TempAllocatorImpl, whose constructor
 * allocates through the function pointer RegisterDefaultAllocator installs, so
 * constructing one with no live JoltLibrary is a call through null - idiom 34's
 * exact crash. Taking the reference is what makes the caller prove it has one.
 */
Runtime::Runtime(const objectmodel::World&, physics::JoltLibrary&) {}

}  // namespace dunya::runtime
