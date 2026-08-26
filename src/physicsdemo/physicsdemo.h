#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>

#include <dunya/physics/physicsworld/physicsworld.h>

class PhysicsDemo {
public:
  explicit PhysicsDemo(dunya::physics::PhysicsWorld& world);
  ~PhysicsDemo();

  PhysicsDemo(const PhysicsDemo&) = delete;
  PhysicsDemo& operator=(const PhysicsDemo&) = delete;
  PhysicsDemo(PhysicsDemo&&) = delete;
  PhysicsDemo& operator=(PhysicsDemo&&) = delete;

  void log() const;

private:
  dunya::physics::PhysicsWorld& m_world;

  JPH::BodyID m_floorId;
  JPH::BodyID m_sphereId;
};
