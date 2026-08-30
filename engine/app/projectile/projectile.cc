#include "projectile.ih"

namespace {

constexpr float PROJECTILE_RADIUS = 0.35f;

constexpr uint32_t PROJECTILE_RESOLUTION = dunya::core::FIELD_GRID_RESOLUTION;

constexpr float PROJECTILE_SPEED = 42.0f;

constexpr float PROJECTILE_MASS = 600.0f;

}

Projectile makeProjectile(
  uint32_t material,
  const dunya::objectmodel::WorldExtent& target
) {
  Projectile shot;

  shot.grid.resolution = glm::uvec3(PROJECTILE_RESOLUTION);

  shot.shape =
    dunya::field::makeSphere(glm::vec3(0.0f), PROJECTILE_RADIUS, material);

  shot.speed = PROJECTILE_SPEED;

  shot.mass = PROJECTILE_MASS;

  if (!target.empty) {
    shot.height = target.centre().y;
    shot.aimAt = target.centre();
  }

  return shot;
}

dunya::field::SampledSdf bakeProjectile(const Projectile& shot) {
  const dunya::field::Aabb box = dunya::objectmodel::gridBox({&shot.shape, 1});

  return dunya::field::bake(
    std::span<const dunya::field::Primitive>(&shot.shape, 1),
    box.minimum,
    box.maximum,
    shot.grid.resolution
  );
}
