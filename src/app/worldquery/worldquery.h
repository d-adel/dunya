#pragma once

#include <dunya/objectmodel/world/world.h>

#include <glm/glm.hpp>

struct WorldExtent {
  glm::vec3 minimum{0.0f};
  glm::vec3 maximum{0.0f};

  bool empty = true;

  [[nodiscard]] glm::vec3 centre() const noexcept;
  [[nodiscard]] glm::vec3 span() const noexcept;
};

[[nodiscard]] WorldExtent dynamicExtent(const dunya::objectmodel::World& world);

[[nodiscard]] WorldExtent staticExtent(const dunya::objectmodel::World& world);

[[nodiscard]] WorldExtent entityExtent(
  const dunya::objectmodel::World& world,
  dunya::objectmodel::Entity entity
);

[[nodiscard]] dunya::objectmodel::Entity firstDeformable(
  const dunya::objectmodel::World& world
);
