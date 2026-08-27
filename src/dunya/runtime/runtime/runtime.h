#pragma once

#include <dunya/objectmodel/world/world.h>
#include <dunya/physics/physicsworld/physicsworld.h>
#include <dunya/physics/joltlibrary/joltlibrary.h>

namespace dunya::runtime {

class Runtime {
public:
  Runtime(const objectmodel::World& source, physics::JoltLibrary&);

  Runtime(const Runtime&) = delete;
  Runtime& operator=(const Runtime&) = delete;
  Runtime(Runtime&&) = delete;
  Runtime& operator=(Runtime&&) = delete;

private:
  // Declaration order matters
  // Destroy per-object table before PhysicsWorld
  objectmodel::World m_world;
  physics::PhysicsWorld m_physicsWorld;
};

}  // namespace dunya::runtime
