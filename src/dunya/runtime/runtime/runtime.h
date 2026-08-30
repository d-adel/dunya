#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>

#include <dunya/objectmodel/renderpose/renderpose.h>
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

  void refreshBody(objectmodel::Entity entity);

  void reshapeAfterDeform(
    objectmodel::Entity entity,
    const glm::uvec3& brickBegin,
    const glm::uvec3& brickEnd
  );

  void wake(const glm::vec3& minimum, const glm::vec3& maximum);

  void setBodyShape(objectmodel::Entity entity, const JPH::ShapeRefC& shape);

  void launch(objectmodel::Entity entity, const glm::vec3& velocity);

  bool despawn(objectmodel::Entity entity);

  void setMass(objectmodel::Entity entity, float mass);

  [[nodiscard]] size_t shapeCount() const noexcept;

  void step();

  void syncPoses(float alpha);

private:
  void applyMassScale(objectmodel::Entity entity);

  struct SharedShape {
    std::weak_ptr<dunya::field::SampledField> lattice;
    JPH::ShapeRefC shape;
  };

  [[nodiscard]] JPH::ShapeRefC shapeFor(const objectmodel::SharedField& held);

  void rememberShape(
    const objectmodel::SharedField& held,
    const JPH::ShapeRefC& shape
  );

  objectmodel::World m_world;
  physics::PhysicsWorld m_physicsWorld;

  std::vector<std::pair<objectmodel::Entity, objectmodel::Pose>> m_poseScratch;
  std::vector<std::pair<objectmodel::Entity, objectmodel::RenderPose>>
    m_renderScratch;

  std::unordered_map<uint32_t, objectmodel::Pose> m_previousPoses;

  std::unordered_map<const dunya::field::SampledField*, SharedShape> m_shapes;

  bool m_masslessReported = false;

  bool m_broadPhaseStale = false;
};

}
