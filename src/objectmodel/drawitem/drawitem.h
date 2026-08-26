#pragma once

#include <glm/glm.hpp>

#include <cstdint>

// Where one mesh is placed, by index rather than by pointer.
struct DrawItem {
  uint32_t meshIndex;
  uint32_t materialIndex;
  glm::mat4 model;
};
