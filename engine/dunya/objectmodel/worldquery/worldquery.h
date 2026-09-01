#pragma once

#include <dunya/field/raycast/raycast.h>
#include <dunya/objectmodel/world/world.h>

#include <dunya/objectmodel/component/lens/lens.h>
#include <dunya/objectmodel/component/pose/pose.h>

#include <glm/glm.hpp>

#include <optional>

namespace dunya::objectmodel {

struct WorldExtent {
  glm::vec3 minimum{0.0f};
  glm::vec3 maximum{0.0f};

  bool empty = true;

  [[nodiscard]] glm::vec3 centre() const noexcept;
  [[nodiscard]] glm::vec3 span() const noexcept;
};

[[nodiscard]] WorldExtent dynamicExtent(const dunya::objectmodel::World& world);

[[nodiscard]] WorldExtent entityExtent(
  const dunya::objectmodel::World& world,
  dunya::objectmodel::Entity entity
);

template<typename... Ts>
[[nodiscard]] dunya::objectmodel::Entity firstWith(
  const dunya::objectmodel::World& world
) {
  const entt::registry& registry = world.registry();

  dunya::objectmodel::Entity found = dunya::objectmodel::INVALID_ENTITY;

  for (const dunya::objectmodel::Entity entity : registry.view<const Ts...>()) {
    if (
      found == dunya::objectmodel::INVALID_ENTITY
      || entt::to_entity(entity) < entt::to_entity(found)
    ) {
      found = entity;
    }
  }

  return found;
}

[[nodiscard]] dunya::objectmodel::Entity firstLens(
  const dunya::objectmodel::World& world
);

[[nodiscard]] dunya::objectmodel::Entity firstDeformable(
  const dunya::objectmodel::World& world
);

struct WorldHit {
  dunya::objectmodel::Entity entity = dunya::objectmodel::INVALID_ENTITY;
  glm::vec3 position{0.0f};
  float travelled = 0.0f;
  uint32_t material = 0u;
};

[[nodiscard]] WorldHit raycastWorld(
  const dunya::objectmodel::World& world,
  const dunya::field::Ray& ray
);

struct Framing {
  glm::vec3 position{0.0f};
  float yaw = 0.0f;
  float pitch = 0.0f;
};

[[nodiscard]] Framing frameExtent(const WorldExtent& extent);

[[nodiscard]] Pose framingPose(const Framing& framing);

struct CameraView {
  glm::mat4 view{1.0f};
  glm::mat4 projection{1.0f};
  glm::vec3 position{0.0f};
  float nearPlane = 0.1f;
};

[[nodiscard]] CameraView cameraView(
  const Pose& pose,
  const Lens& lens,
  float aspect
);

[[nodiscard]] std::optional<CameraView> activeCamera(
  const dunya::objectmodel::World& world,
  float aspect
);

[[nodiscard]] std::optional<dunya::field::Ray> screenPointToRay(
  const dunya::objectmodel::World& world,
  dunya::objectmodel::Entity camera,
  const glm::vec2& screen,
  const glm::vec2& viewport
);

[[nodiscard]] CameraView framingCamera(
  const dunya::objectmodel::World& world,
  float aspect
);

}
