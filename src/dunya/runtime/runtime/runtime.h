#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>

#include <dunya/objectmodel/world/world.h>
#include <dunya/physics/physicsworld/physicsworld.h>
#include <dunya/physics/joltlibrary/joltlibrary.h>

#include <memory>
#include <unordered_map>

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

  // Rebuilds the body over the same grid, recomputing only the bricks a write
  // reported. The full walk is two million cells and a Newton solve per
  // surface brick; a deformation moves a handful of them. Falls back to
  // refreshBody when the body is not on a shape over this field.
  void reshapeAfterDeform(
    objectmodel::Entity entity,
    const glm::uvec3& brickBegin,
    const glm::uvec3& brickEnd
  );

  // Wakes whatever was resting on geometry that has just changed. Jolt only
  // invalidates the contact cache of the body whose shape was swapped, so a
  // box asleep on top of a fresh crater would never notice it.
  void wake(const glm::vec3& minimum, const glm::vec3& maximum);

  // Creates or re-shapes this entity's body on a shape the caller owns, which
  // is how objects that are the same object share one: a shape is immutable
  // and refcounted. The field it borrows must outlive every body built on it.
  void setBodyShape(objectmodel::Entity entity, const JPH::ShapeRefC& shape);

  // Gives a body a velocity and wakes it. Velocity is Jolt's once a body
  // exists, so this is the only way in and there is no component mirroring it.
  void launch(objectmodel::Entity entity, const glm::vec3& velocity);

  // Destroys a body and the entity that named it. The volume slot it held is
  // reclaimed by the frame loop, which already sweeps slots whose entity is
  // gone.
  bool despawn(objectmodel::Entity entity);

  // Scales a body to a mass, inertia with it. What is remembered is the
  // factor, not the weight, so a later rebake still leaves a carved body
  // lighter — the shape deriving mass from its volume is the point.
  void setMass(objectmodel::Entity entity, float mass);

  // How many distinct collision shapes the bodies are built on. The lattice
  // count beside it says how much of the sharing is the lattice and how much
  // is the shape derived from it.
  [[nodiscard]] size_t shapeCount() const noexcept;

  // One fixed step of the simulation.
  void step();

  // Pulls the transforms of everything that moved this frame back into the
  // world. Once per frame, after the step loop, never per step.
  void syncPoses();

private:
  // Puts back what a fresh shape's mass properties just overwrote. Nothing
  // happens without a MassScale, which is most bodies.
  void applyMassScale(objectmodel::Entity entity);

  // One shape per lattice rather than one per object. A FieldShape is a pure
  // function of the lattice it reads - the same mass walk, the same contact
  // seeds - and a Jolt shape is immutable and refcounted, so six hundred
  // crates cut from the same primitives want one between them.
  //
  // The weak reference is what makes the address safe as a key: a lattice
  // freed and another allocated where it was reads as a miss rather than as
  // somebody else's geometry.
  struct SharedShape {
    std::weak_ptr<dunya::field::SampledField> lattice;
    JPH::ShapeRefC shape;
  };

  [[nodiscard]] JPH::ShapeRefC shapeFor(const objectmodel::SharedField& held);

  // The current shape for a lattice, which a deformation changes. Without
  // this the cache would keep handing out the shape from before the dent.
  void rememberShape(
    const objectmodel::SharedField& held,
    const JPH::ShapeRefC& shape
  );

  // Declaration order is load-bearing: m_physicsWorld holds bodies that refer
  // to entities in m_world, and members are destroyed in reverse declaration
  // order, so the world must outlive the simulation that points into it.
  objectmodel::World m_world;
  physics::PhysicsWorld m_physicsWorld;

  std::vector<std::pair<objectmodel::Entity, objectmodel::Pose>> m_poseScratch;

  std::unordered_map<const dunya::field::SampledField*, SharedShape> m_shapes;

  // Once per run, not once per frame: an object with nothing solid in it stays
  // that way, and the frame loop would otherwise say so sixty times a second.
  bool m_masslessReported = false;

  // Bodies arrive a frame after the world does, so the broad phase is rebuilt
  // on the next step rather than once at construction.
  bool m_broadPhaseStale = false;
};

}  // namespace dunya::runtime
