#pragma once

#include <dunya/field/field.h>

#include <glm/glm.hpp>

#include <cstdint>
#include <span>
#include <vector>

namespace dunya::field {

/* A distance field baked onto a regular lattice.
 *
 * Values sit at lattice points, not voxel centres, so a resolution of N spans
 * N-1 cells along each axis and a sample taken exactly at a lattice point
 * returns the baked value untouched.
 *
 * The grid is bounded and the field it came from may not be, so sampling
 * outside returns the distance to the grid's box. That is a valid lower bound
 * on the distance to anything inside it, which is what keeps a ray marching
 * toward the grid rather than stepping past it.
 */

struct SampledField {
  glm::vec3 origin{0.0f};
  glm::vec3 voxelSize{1.0f};
  glm::uvec3 resolution{0u};

  std::vector<float> distances;
  std::vector<uint32_t> materials;
};

// The spacing a lattice of this resolution has across this box. The bake and
// the pass that re-places the grid after an edit both need it, and one of them
// writing the division itself is how the two drift apart.
glm::vec3 voxelSize(
  const glm::vec3& minimum,
  const glm::vec3& maximum,
  const glm::uvec3& resolution
);

SampledField bake(
  std::span<const Primitive> primitives,
  const glm::vec3& minimum,
  const glm::vec3& maximum,
  const glm::uvec3& resolution
);

// The representation is the overload parameter, so this joins the analytic
// sample() without either being renamed.
FieldSample sample(const SampledField& field, const glm::vec3& point);

}  // namespace dunya::field
