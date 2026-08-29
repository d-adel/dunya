#pragma once

#include <dunya/objectmodel/world/world.h>
#include <dunya/physics/impact/impact.h>
#include <dunya/renderer/frame/frame.h>
#include <scene/scene.h>
#include <dunya/runtime/deformation/deformation.h>

#include <vulkan/vulkan.h>

#include <cstddef>
#include <functional>

// The debug panels, one function per panel body.
//
// Free functions taking what they read, rather than lambdas capturing an
// Application: a panel that captures `this` can reach anything, so it ends up
// living next to everything, and 195 lines of them is how a frame loop's class
// acquires a UI. Each of these declares its own surface instead, and the
// registration is what stays behind.
//
// They stay in the executable and out of the libraries. Nothing under
// src/dunya/ names ImGui, and a panel is a workbench tool: the game build
// carries neither.
namespace panels {

// The demo's headline. Two numbers that are the whole claim - the primitive
// count and the resident bytes - neither of which moves however much of the
// wall is gone.
void dunya(
  const dunya::objectmodel::World& world,
  const dunya::runtime::Deformation& deformation,
  bool playing,
  double frameMs
);

// How a contact impulse becomes a crater. `impacts` is null while nothing is
// simulating, because the threshold lives on the listener and there is none.
void damage(
  dunya::runtime::Deformation& deformation,
  dunya::physics::ImpactListener* impacts
);

// What the panel can do beyond writing to the settings it was handed. An empty
// callback is a disabled button rather than a branch inside the panel.
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

// The primitive count arrives already chosen, which is the point: this used to
// read the authored world while its sibling read the active one, so during Play
// the two panels disagreed and this was the wrong one.
void frame(double frameMs, VkExtent2D extent, size_t primitives, bool analytic);

void march(dunya::renderer::MarchParams& march);

}  // namespace panels
