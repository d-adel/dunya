#pragma once

#include <glm/glm.hpp>

#include <cstdint>

namespace dunya::renderer {

// One mesh draw, assembled for this frame from an entity's Mesh, Material and
// Pose. The matrix is computed rather than stored, which is what DrawItem got
// wrong: placement, geometry and appearance were three concerns in one struct.
struct MeshRecord {
  uint32_t mesh = UINT32_MAX;
  uint32_t material = UINT32_MAX;
  glm::mat4 model = glm::mat4(1.0f);
};

}  // namespace dunya::renderer
