#pragma once

#include <app/worldquery/worldquery.h>
#include <dunya/field/field.h>
#include <dunya/field/sampled/sampled.h>
#include <dunya/objectmodel/component/sdfgrid/sdfgrid.h>

#include <cstdint>

#include <glm/glm.hpp>

struct Projectile {
  dunya::objectmodel::SdfGrid grid;
  dunya::field::Primitive shape;

  float speed = 0.0f;

  float height = 0.0f;
  glm::vec3 aimAt{0.0f};

  float mass = 100.0f;
};

[[nodiscard]] Projectile makeProjectile(
  uint32_t material,
  const WorldExtent& target
);

[[nodiscard]] dunya::field::SampledField bakeProjectile(const Projectile& shot);
