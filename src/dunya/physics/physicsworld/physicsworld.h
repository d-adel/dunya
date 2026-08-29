#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>

#include <dunya/physics/impact/impact.h>
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

  // Jolt holds one contact listener and it has to outlive every Update, so it
  // is a member rather than something a caller registers and has to keep
  // alive. What it records is not deformation-specific - an impact is also
  // what a sound or a particle burst would key off.
  ImpactListener& impacts() noexcept;

private:
  BroadPhaseLayerInterface m_broadPhaseLayerInterface;
  ObjectVsBroadPhaseLayerFilter m_objectVsBroadPhaseLayerFilter;
  ObjectLayerPairFilter m_objectLayerPairFilter;

  JPH::TempAllocatorImpl m_tempAllocator;
  JPH::JobSystemThreadPool m_jobSystem;

  // Declared before the system, because the system holds a pointer to it and
  // members are destroyed in reverse declaration order.
  ImpactListener m_impacts;

  // Reported once, not once a frame: a full cache stays full.
  bool m_updateErrorReported = false;

  JPH::PhysicsSystem m_system;
};

}  // namespace dunya::physics
