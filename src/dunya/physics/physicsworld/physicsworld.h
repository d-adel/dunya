#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>

#include <dunya/physics/layers/layers.h>

namespace dunya::physics {

class PhysicsWorld {
public:
  static constexpr float TIME_STEP = 1.0f / 60.0f;

  PhysicsWorld();
  ~PhysicsWorld();

  PhysicsWorld(const PhysicsWorld&) = delete;
  PhysicsWorld& operator=(const PhysicsWorld&) = delete;
  PhysicsWorld(PhysicsWorld&&) = delete;
  PhysicsWorld& operator=(PhysicsWorld&&) = delete;

  void step();
  void optimizeBroadPhase();

  JPH::BodyInterface& bodies();
  const JPH::BodyInterface& bodies() const;

  // The active-body list lives on the system, not the body interface.
  JPH::PhysicsSystem& system();
  const JPH::PhysicsSystem& system() const;

private:
  BroadPhaseLayerInterface m_broadPhaseLayerInterface;
  ObjectVsBroadPhaseLayerFilter m_objectVsBroadPhaseLayerFilter;
  ObjectLayerPairFilter m_objectLayerPairFilter;

  JPH::TempAllocatorImpl m_tempAllocator;
  JPH::JobSystemThreadPool m_jobSystem;

  JPH::PhysicsSystem m_system;
};

}  // namespace dunya::physics
