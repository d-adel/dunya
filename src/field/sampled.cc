#include "sampled.ih"

namespace dunya::field {

namespace {

uint32_t latticeIndex(const SampledField& field, const glm::uvec3& at) {
  return at.x + field.resolution.x * (at.y + field.resolution.y * at.z);
}

glm::vec3 maximumCorner(const SampledField& field) {
  return field.origin
         + field.voxelSize * glm::vec3(field.resolution - glm::uvec3(1u));
}

}  // namespace

glm::vec3 voxelSize(
  const glm::vec3& minimum,
  const glm::vec3& maximum,
  const glm::uvec3& resolution
) {
  return (maximum - minimum) / glm::vec3(resolution - glm::uvec3(1u));
}

SampledField bake(
  std::span<const Primitive> primitives,
  const glm::vec3& minimum,
  const glm::vec3& maximum,
  const glm::uvec3& resolution
) {
  if (resolution.x < 2u || resolution.y < 2u || resolution.z < 2u) {
    throw std::runtime_error(
      "A sampled field needs at least two lattice "
      "points on every axis"
    );
  }

  SampledField field;
  field.origin = minimum;
  field.resolution = resolution;
  field.voxelSize = voxelSize(minimum, maximum, resolution);

  const size_t count =
    static_cast<size_t>(resolution.x) * resolution.y * resolution.z;

  field.distances.resize(count);
  field.materials.resize(count);

  for (uint32_t z = 0; z < resolution.z; ++z) {
    for (uint32_t y = 0; y < resolution.y; ++y) {
      for (uint32_t x = 0; x < resolution.x; ++x) {
        const glm::vec3 point =
          field.origin + field.voxelSize * glm::vec3(x, y, z);

        const FieldSample baked = sample(primitives, point);
        const uint32_t index = latticeIndex(field, glm::uvec3(x, y, z));

        field.distances[index] = baked.distance;
        field.materials[index] = baked.material;
      }
    }
  }

  return field;
}

FieldSample sample(const SampledField& field, const glm::vec3& point) {
  const glm::vec3 clamped =
    glm::clamp(point, field.origin, maximumCorner(field));

  const float outside = glm::length(point - clamped);

  if (outside > 0.0f) {
    // Distance to the box the grid covers. Conservative: nothing inside can be
    // nearer than this, so a march steps up to the grid and no further.
    return {outside, 0};
  }

  const glm::vec3 lattice = (point - field.origin) / field.voxelSize;

  // The cell whose far corner is still in range, so base + 1 is always valid.
  const glm::uvec3 base = glm::min(
    glm::uvec3(glm::floor(lattice)),
    field.resolution - glm::uvec3(2u)
  );

  const glm::vec3 t = lattice - glm::vec3(base);

  float corner[8];
  for (uint32_t i = 0; i < 8u; ++i) {
    const glm::uvec3 offset(i & 1u, (i >> 1) & 1u, (i >> 2) & 1u);
    corner[i] = field.distances[latticeIndex(field, base + offset)];
  }

  const float x00 = glm::mix(corner[0], corner[1], t.x);
  const float x10 = glm::mix(corner[2], corner[3], t.x);
  const float x01 = glm::mix(corner[4], corner[5], t.x);
  const float x11 = glm::mix(corner[6], corner[7], t.x);

  const float y0 = glm::mix(x00, x10, t.y);
  const float y1 = glm::mix(x01, x11, t.y);

  // Material ids are names, not quantities: interpolating them would invent a
  // material that does not exist, so the nearest lattice point wins.
  const glm::uvec3 nearest = base
                             + glm::uvec3(
                               t.x < 0.5f ? 0u : 1u,
                               t.y < 0.5f ? 0u : 1u,
                               t.z < 0.5f ? 0u : 1u
                             );

  return {glm::mix(y0, y1, t.z), field.materials[latticeIndex(field, nearest)]};
}

}  // namespace dunya::field
