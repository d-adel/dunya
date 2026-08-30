#pragma once

#include <dunya/objectmodel/worldquery/worldquery.h>
#include <dunya/field/field.h>
#include <dunya/field/sampledsdf/sampledsdf.h>
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
  const dunya::objectmodel::WorldExtent& target
);

[[nodiscard]] dunya::field::SampledSdf bakeProjectile(const Projectile& shot);
