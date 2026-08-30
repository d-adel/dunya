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

  JPH::PhysicsSystem& system();
  const JPH::PhysicsSystem& system() const;

  ImpactListener& impacts() noexcept;

private:
  BroadPhaseLayerInterface m_broadPhaseLayerInterface;
  ObjectVsBroadPhaseLayerFilter m_objectVsBroadPhaseLayerFilter;
  ObjectLayerPairFilter m_objectLayerPairFilter;

  JPH::TempAllocatorImpl m_tempAllocator;
  JPH::JobSystemThreadPool m_jobSystem;

  ImpactListener m_impacts;

  bool m_updateErrorReported = false;

  JPH::PhysicsSystem m_system;
};

}  // namespace dunya::physics
