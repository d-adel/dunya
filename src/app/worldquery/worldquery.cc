#include "worldquery.ih"

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

WorldExtent extentOf(const dunya::objectmodel::World& world, bool wantStatic) {
  const entt::registry& registry = world.registry();

  WorldExtent extent;

  for (const dunya::objectmodel::Entity entity : world.fields()) {
    const bool isStatic =
      registry.all_of<dunya::objectmodel::StaticBody>(entity);

    if (isStatic != wantStatic) {
      continue;
    }

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

WorldExtent dynamicExtent(const dunya::objectmodel::World& world) {
  return extentOf(world, false);
}

WorldExtent staticExtent(const dunya::objectmodel::World& world) {
  return extentOf(world, true);
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
