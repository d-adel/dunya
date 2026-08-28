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

  objectmodel::World& world() noexcept;
  const objectmodel::World& world() const noexcept;

  physics::PhysicsWorld& physics() noexcept;

private:
  // Declaration order is load-bearing: m_physicsWorld holds bodies that refer
  // to entities in m_world, and members are destroyed in reverse declaration
  // order, so the world must outlive the simulation that points into it.
  objectmodel::World m_world;
  physics::PhysicsWorld m_physicsWorld;
};

}  // namespace dunya::runtime
