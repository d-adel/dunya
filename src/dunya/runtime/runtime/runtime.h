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

  // Brings one entity's body in line with its field: creates the body the
  // first time a SampledField appears, and rebuilds the shape when a rebake
  // has replaced the field the old shape reads. Does nothing without a field.
  void refreshBody(objectmodel::Entity entity);

  // One fixed step of the simulation.
  void step();

  // Pulls the transforms of everything that moved this frame back into the
  // world. Once per frame, after the step loop, never per step.
  void syncPoses();

private:
  // Declaration order is load-bearing: m_physicsWorld holds bodies that refer
  // to entities in m_world, and members are destroyed in reverse declaration
  // order, so the world must outlive the simulation that points into it.
  objectmodel::World m_world;
  physics::PhysicsWorld m_physicsWorld;

  std::vector<std::pair<objectmodel::Entity, objectmodel::Pose>> m_poseScratch;

  // Bodies arrive a frame after the world does, so the broad phase is rebuilt
  // on the next step rather than once at construction.
  bool m_broadPhaseStale = false;
};

}  // namespace dunya::runtime
