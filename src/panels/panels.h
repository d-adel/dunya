#pragma once

#include <dunya/objectmodel/world/world.h>
#include <dunya/physics/impact/impact.h>
#include <dunya/renderer/frame/frame.h>
#include <scene/scene.h>
#include <dunya/runtime/deformation/deformation.h>

#include <vulkan/vulkan.h>

#include <cstddef>
#include <functional>

namespace panels {

void dunya(
  const dunya::objectmodel::World& world,
  const dunya::runtime::Deformation& deformation,
  bool playing,
  double frameMs
);

void damage(
  dunya::runtime::Deformation& deformation,
  dunya::physics::ImpactListener* impacts
);

struct ShotActions {
  std::function<void()> fire;
  std::function<void()> resetWall;
};

void shot(
  Scene::Projectile& settings,
  size_t balls,
  size_t maxBalls,
  const ShotActions& actions
);

void frame(double frameMs, VkExtent2D extent, size_t primitives, bool analytic);

void march(dunya::renderer::MarchParams& march);

}
