#include "worldquery.ih"

namespace dunya::objectmodel {

namespace {

void absorb(WorldExtent& extent, const glm::vec3& point) {
  if (extent.empty) {
    extent.minimum = point;
    extent.maximum = point;
    extent.empty = false;

    return;
  }

  extent.minimum = glm::min(extent.minimum, point);
  extent.maximum = glm::max(extent.maximum, point);
}

void absorbSolid(
  WorldExtent& extent,
  const dunya::objectmodel::Pose& pose,
  std::span<const dunya::field::Primitive> primitives
) {
  const std::optional<dunya::field::Aabb> solid =
    dunya::field::boundedExtent(primitives);

  if (!solid.has_value()) {
    return;
  }

  const glm::mat4 model = dunya::objectmodel::model(pose);

  for (uint32_t corner = 0u; corner < 8u; ++corner) {
    const glm::vec3 local(
      (corner & 1u) == 0u ? solid->minimum.x : solid->maximum.x,
      (corner & 2u) == 0u ? solid->minimum.y : solid->maximum.y,
      (corner & 4u) == 0u ? solid->minimum.z : solid->maximum.z
    );

    absorb(extent, glm::vec3(model * glm::vec4(local, 1.0f)));
  }
}

WorldExtent extentOf(const dunya::objectmodel::World& world) {
  const entt::registry& registry = world.registry();

  WorldExtent extent;

  for (const dunya::objectmodel::Entity entity : world.fields()) {
    absorbSolid(
      extent,
      registry.get<dunya::objectmodel::Pose>(entity),
      world.primitives(entity)
    );
  }

  return extent;
}

}

glm::vec3 WorldExtent::centre() const noexcept {
  return 0.5f * (minimum + maximum);
}

glm::vec3 WorldExtent::span() const noexcept {
  return maximum - minimum;
}

dunya::objectmodel::Entity firstLens(const dunya::objectmodel::World& world) {
  return firstWith<dunya::objectmodel::Lens, dunya::objectmodel::Pose>(world);
}

dunya::objectmodel::Entity mainCamera(const dunya::objectmodel::World& world) {
  const entt::registry& registry = world.registry();

  for (const dunya::objectmodel::Entity tagged :
       registry.view<const dunya::objectmodel::MainCamera>()) {
    if (
      registry.all_of<dunya::objectmodel::Lens, dunya::objectmodel::Pose>(
        tagged
      )
    ) {
      return tagged;
    }
  }

  return dunya::objectmodel::INVALID_ENTITY;
}

WorldExtent sceneExtent(const dunya::objectmodel::World& world) {
  return extentOf(world);
}

WorldExtent entityExtent(
  const dunya::objectmodel::World& world,
  dunya::objectmodel::Entity entity
) {
  const entt::registry& registry = world.registry();

  WorldExtent extent;

  if (!registry.all_of<dunya::objectmodel::SdfGrid>(entity)) {
    return extent;
  }

  absorbSolid(
    extent,
    registry.get<dunya::objectmodel::Pose>(entity),
    world.primitives(entity)
  );

  return extent;
}

dunya::objectmodel::Entity firstDeformable(
  const dunya::objectmodel::World& world
) {
  const entt::registry& registry = world.registry();

  for (const dunya::objectmodel::Entity entity : world.fields()) {
    if (
      registry.all_of<
        dunya::objectmodel::Deformable,
        dunya::objectmodel::StaticBody>(entity)
    ) {
      return entity;
    }
  }

  return dunya::objectmodel::INVALID_ENTITY;
}

WorldHit raycastWorld(
  const dunya::objectmodel::World& world,
  const dunya::field::Ray& ray
) {
  const entt::registry& registry = world.registry();

  WorldHit nearest{};

  for (const dunya::objectmodel::Entity entity : world.fields()) {
    const std::span<const dunya::field::Primitive> primitives =
      world.primitives(entity);

    if (primitives.empty()) {
      continue;
    }

    const glm::mat4 inverseModel = glm::inverse(
      dunya::objectmodel::model(registry.get<dunya::objectmodel::Pose>(entity))
    );

    const dunya::field::Ray local{
      glm::vec3(inverseModel * glm::vec4(ray.origin, 1.0f)),
      glm::vec3(inverseModel * glm::vec4(ray.direction, 0.0f))
    };

    const dunya::field::Aabb box = dunya::objectmodel::gridBox(
      primitives,
      dunya::objectmodel::gridMargin(
        registry.get<dunya::objectmodel::SdfGrid>(entity),
        primitives
      )
    );

    if (!dunya::field::intersect(box, local).has_value()) {
      continue;
    }

    const std::optional<dunya::field::RayHit> hit =
      dunya::field::raymarch(primitives, local);

    if (!hit.has_value()) {
      continue;
    }

    if (
      nearest.entity != dunya::objectmodel::INVALID_ENTITY
      && hit->travelled >= nearest.travelled
    ) {
      continue;
    }

    nearest.entity = entity;
    nearest.travelled = hit->travelled;
    nearest.material = hit->material;
    nearest.position = ray.origin + ray.direction * hit->travelled;
  }

  return nearest;
}

Framing frameExtent(const WorldExtent& extent) {
  if (extent.empty) {
    return {};
  }

  const glm::vec3 span = extent.span();

  constexpr float HALF_FOV = glm::radians(35.0f);

  const float reach = 0.5f * std::max(span.x, span.y) / std::tan(HALF_FOV);

  const float distance = reach + 3.0f;

  constexpr float PITCH = glm::radians(-20.0f);

  return {
    glm::vec3(0.0f, extent.centre().y + distance * -std::sin(PITCH), distance),
    0.0f,
    PITCH
  };
}

Pose framingPose(const Framing& framing) {
  const glm::quat yaw =
    glm::angleAxis(framing.yaw, glm::vec3(0.0f, -1.0f, 0.0f));
  const glm::quat pitch =
    glm::angleAxis(framing.pitch, glm::vec3(1.0f, 0.0f, 0.0f));

  Pose pose{};
  pose.position = framing.position;
  pose.rotation = glm::normalize(yaw * pitch);

  return pose;
}

CameraView cameraView(const Pose& pose, const Lens& lens, float aspect) {
  CameraView resolved{};
  resolved.view = view(pose);
  resolved.projection = projection(lens, aspect);
  resolved.position = pose.position;
  resolved.nearPlane = lens.nearPlane;

  return resolved;
}

std::optional<CameraView> activeCamera(
  const dunya::objectmodel::World& world,
  float aspect
) {
  const dunya::objectmodel::Entity eye = mainCamera(world);

  if (eye == dunya::objectmodel::INVALID_ENTITY) {
    return std::nullopt;
  }

  const entt::registry& registry = world.registry();

  return cameraView(
    registry.get<const Pose>(eye),
    registry.get<const Lens>(eye),
    aspect
  );
}

std::optional<dunya::field::Ray> screenPointToRay(
  const dunya::objectmodel::World& world,
  dunya::objectmodel::Entity camera,
  const glm::vec2& screen,
  const glm::vec2& viewport
) {
  if (viewport.x <= 0.0f || viewport.y <= 0.0f) {
    return std::nullopt;
  }

  const entt::registry& registry = world.registry();

  if (!registry.valid(camera) || !registry.all_of<Pose, Lens>(camera)) {
    return std::nullopt;
  }

  const CameraView seat = cameraView(
    registry.get<const Pose>(camera),
    registry.get<const Lens>(camera),
    viewport.x / viewport.y
  );

  const glm::vec2 ndc = 2.0f * screen / viewport - 1.0f;

  return dunya::field::screenPointToRay(
    glm::inverse(seat.projection * seat.view),
    seat.position,
    ndc
  );
}

CameraView framingCamera(const dunya::objectmodel::World& world, float aspect) {
  const WorldExtent target = sceneExtent(world);

  if (target.empty) {
    return cameraView(Pose{}, Lens{}, aspect);
  }

  return cameraView(framingPose(frameExtent(target)), Lens{}, aspect);
}

}
