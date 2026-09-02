#pragma once

#include <dunya/objectmodel/world/world.h>
#include <dunya/objectmodel/worldquery/worldquery.h>
#include <dunya/view/viewport/viewport.h>

namespace dunya::view {

[[nodiscard]] dunya::objectmodel::CameraView lookThrough(
  const Viewport& port,
  const dunya::objectmodel::World& world,
  float aspect
);

[[nodiscard]] bool bindingIsLive(
  const Viewport& port,
  const dunya::objectmodel::World& world
);

}
