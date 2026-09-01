#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>

#include <dunya/objectmodel/component/renderpose/renderpose.h>
#include <dunya/objectmodel/world/world.h>
#include <dunya/physics/impact/impact.h>
#include <dunya/physics/physicsworld/physicsworld.h>
#include <dunya/physics/joltlibrary/joltlibrary.h>

#include <memory>
#include <span>
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

  void refreshBody(objectmodel::Entity entity);

  void reshapeAfterDeform(
    objectmodel::Entity entity,
    const glm::uvec3& brickBegin,
    const glm::uvec3& brickEnd
  );

  void wake(const glm::vec3& minimum, const glm::vec3& maximum);

  struct Damage {
    float threshold = 3.0f;

    float depthPerImpulse = 0.0002f;
    float minimumDepth = 0.03f;
    float maximumDepth = 0.25f;

    float radiusPerDepth = 2.5f;

    uint32_t perFrame = 3u;

    float widestFraction = 0.25f;
  };

  struct Crater {
    objectmodel::Entity entity{};
    float impulse = 0.0f;
    float depth = 0.0f;
    float radius = 0.0f;
  };

  [[nodiscard]] Damage& damage() noexcept;
  [[nodiscard]] const Damage& damage() const noexcept;

  void carve(objectmodel::Entity entity, const field::Primitive& cutter);

  void applyImpacts();

  [[nodiscard]] std::span<const Crater> cratersThisFrame() const noexcept;

  void refreshDeformedBodies();

  void setBodyShape(objectmodel::Entity entity, const JPH::ShapeRefC& shape);

  void setLinearVelocity(objectmodel::Entity entity, const glm::vec3& velocity);

  bool destroy(objectmodel::Entity entity);

  void setMass(objectmodel::Entity entity, float mass);

  [[nodiscard]] size_t shapeCount() const noexcept;

  void step();

  void syncPoses(float alpha);

private:
  struct PendingCrater {
    objectmodel::Entity entity{};
    glm::vec3 point{0.0f};
    glm::vec3 outward{0.0f};
    float impulse = 0.0f;
  };

  Damage m_damage;

  std::vector<physics::Impact> m_impacts;
  std::vector<PendingCrater> m_pending;
  std::vector<Crater> m_carved;

  void applyMassScale(objectmodel::Entity entity);

  struct SharedShape {
    std::weak_ptr<dunya::field::SampledSdf> lattice;
    JPH::ShapeRefC shape;
  };

  [[nodiscard]] JPH::ShapeRefC shapeFor(const objectmodel::SharedSdf& held);

  void rememberShape(
    const objectmodel::SharedSdf& held,
    const JPH::ShapeRefC& shape
  );

  objectmodel::World m_world;
  physics::PhysicsWorld m_physicsWorld;

  std::vector<std::pair<objectmodel::Entity, objectmodel::Pose>> m_poseScratch;
  std::vector<std::pair<objectmodel::Entity, objectmodel::RenderPose>>
    m_renderScratch;

  std::unordered_map<uint32_t, objectmodel::Pose> m_previousPoses;

  std::unordered_map<const dunya::field::SampledSdf*, SharedShape> m_shapes;

  bool m_masslessReported = false;

  bool m_broadPhaseStale = false;
};

}
