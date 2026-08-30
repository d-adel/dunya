#include "sampledsdf.ih"

namespace dunya::field {

namespace {

constexpr float GRADIENT_FLOOR = 1e-6f;

uint32_t latticeIndex(const SampledSdf& field, const glm::uvec3& at) {
  return at.x + field.resolution.x * (at.y + field.resolution.y * at.z);
}

glm::vec3 maximumCorner(const SampledSdf& field) {
  return field.origin
         + field.voxelSize * glm::vec3(field.resolution - glm::uvec3(1u));
}

float outsideDistance(const SampledSdf& field, const glm::vec3& point) {
  const glm::vec3 clamped =
    glm::clamp(point, field.origin, maximumCorner(field));

  return glm::length(point - clamped);
}

struct Cell {
  glm::uvec3 base;
  glm::vec3 t;
};

glm::uvec3 cellCounts(const SampledSdf& field) {
  return field.resolution - glm::uvec3(1u);
}

uint32_t brickIndex(const glm::uvec3& counts, const glm::uvec3& at) {
  return at.x + counts.x * (at.y + counts.y * at.z);
}

void readCorners(
  const SampledSdf& field,
  const glm::uvec3& base,
  float (&corner)[8]
) {
  for (uint32_t i = 0; i < 8u; ++i) {
    const glm::uvec3 offset(i & 1u, (i >> 1) & 1u, (i >> 2) & 1u);
    corner[i] = field.distances[latticeIndex(field, base + offset)];
  }
}

float cellLipschitz(const SampledSdf& field, const glm::uvec3& cell) {
  float corner[8];
  readCorners(field, cell, corner);

  glm::vec3 slope(0.0f);

  for (uint32_t i = 0; i < 8u; ++i) {
    if ((i & 1u) == 0u) {
      slope.x = std::max(slope.x, std::abs(corner[i | 1u] - corner[i]));
    }
    if ((i & 2u) == 0u) {
      slope.y = std::max(slope.y, std::abs(corner[i | 2u] - corner[i]));
    }
    if ((i & 4u) == 0u) {
      slope.z = std::max(slope.z, std::abs(corner[i | 4u] - corner[i]));
    }
  }

  return glm::length(slope / field.voxelSize);
}

struct BrickRange {
  glm::uvec3 begin{0u};
  glm::uvec3 end{0u};
};

BrickRange rebuildBricks(
  SampledSdf& field,
  const glm::uvec3& cellMinimum,
  const glm::uvec3& cellMaximum
) {
  const glm::uvec3 cells = cellCounts(field);
  const glm::uvec3 bricks = brickCounts(field);

  const glm::uvec3 first = cellMinimum / glm::uvec3(BRICK_CELLS);
  const glm::uvec3 last = cellMaximum / glm::uvec3(BRICK_CELLS);

  for (uint32_t bz = first.z; bz <= last.z; ++bz) {
    for (uint32_t by = first.y; by <= last.y; ++by) {
      for (uint32_t bx = first.x; bx <= last.x; ++bx) {
        const glm::uvec3 base =
          glm::uvec3(bx, by, bz) * glm::uvec3(BRICK_CELLS);

        const glm::uvec3 start =
          glm::max(base, glm::uvec3(1u)) - glm::uvec3(1u);
        const glm::uvec3 end =
          glm::min(base + glm::uvec3(BRICK_CELLS + 1u), cells);

        float worst = 0.0f;

        for (uint32_t z = start.z; z < end.z; ++z) {
          for (uint32_t y = start.y; y < end.y; ++y) {
            for (uint32_t x = start.x; x < end.x; ++x) {
              worst =
                std::max(worst, cellLipschitz(field, glm::uvec3(x, y, z)));
            }
          }
        }

        float lowest = std::numeric_limits<float>::max();
        float highest = std::numeric_limits<float>::lowest();

        for (uint32_t z = start.z; z <= end.z; ++z) {
          for (uint32_t y = start.y; y <= end.y; ++y) {
            for (uint32_t x = start.x; x <= end.x; ++x) {
              const float value =
                field.distances[latticeIndex(field, glm::uvec3(x, y, z))];

              lowest = std::min(lowest, value);
              highest = std::max(highest, value);
            }
          }
        }

        const uint32_t brick = brickIndex(bricks, glm::uvec3(bx, by, bz));

        field.brickMinimum[brick] = lowest;
        field.brickMaximum[brick] = highest;
        field.brickLipschitz[brickIndex(bricks, glm::uvec3(bx, by, bz))] =
          worst;
      }
    }
  }

  field.globalLipschitz = 0.0f;

  for (float bound : field.brickLipschitz) {
    field.globalLipschitz = std::max(field.globalLipschitz, bound);
  }

  return {first, last + glm::uvec3(1u)};
}

void brickBox(
  const SampledSdf& field,
  const glm::uvec3& brick,
  glm::vec3& minimum,
  glm::vec3& maximum
) {
  const glm::uvec3 base = brick * glm::uvec3(BRICK_CELLS);
  const glm::uvec3 end =
    glm::min(base + glm::uvec3(BRICK_CELLS), cellCounts(field));

  minimum = field.origin + field.voxelSize * glm::vec3(base);
  maximum = field.origin + field.voxelSize * glm::vec3(end);
}

Cell locate(const SampledSdf& field, const glm::vec3& point) {
  const glm::vec3 lattice = (point - field.origin) / field.voxelSize;

  const glm::uvec3 base = glm::min(
    glm::uvec3(glm::floor(lattice)),
    field.resolution - glm::uvec3(2u)
  );

  return {base, lattice - glm::vec3(base)};
}

}

SampleBox merge(const SampleBox& first, const SampleBox& second) {
  if (glm::any(glm::equal(first.extent, glm::uvec3(0u)))) {
    return second;
  }

  if (glm::any(glm::equal(second.extent, glm::uvec3(0u)))) {
    return first;
  }

  const glm::uvec3 minimum = glm::min(first.minimum, second.minimum);

  const glm::uvec3 beyond =
    glm::max(first.minimum + first.extent, second.minimum + second.extent);

  return {minimum, beyond - minimum};
}

glm::vec3 voxelSize(
  const glm::vec3& minimum,
  const glm::vec3& maximum,
  const glm::uvec3& resolution
) {
  return (maximum - minimum) / glm::vec3(resolution - glm::uvec3(1u));
}

SampledSdf bake(
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

  SampledSdf field;
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

        const AnalyticSample baked = sample(primitives, point);
        const uint32_t index = latticeIndex(field, glm::uvec3(x, y, z));

        field.distances[index] = baked.distance;
        field.materials[index] = static_cast<uint8_t>(baked.material);
      }
    }
  }

  const glm::uvec3 bricks = brickCounts(field);

  field.brickLipschitz.resize(
    static_cast<size_t>(bricks.x) * bricks.y * bricks.z
  );

  field.brickMinimum.resize(
    static_cast<size_t>(bricks.x) * bricks.y * bricks.z
  );

  field.brickMaximum.resize(
    static_cast<size_t>(bricks.x) * bricks.y * bricks.z
  );

  rebuildBricks(field, glm::uvec3(0u), cellCounts(field) - glm::uvec3(1u));

  return field;
}

float distance(const SampledSdf& field, const glm::vec3& point) {
  const float outside = outsideDistance(field, point);

  if (outside > 0.0f) {
    return outside;
  }

  const Cell cell = locate(field, point);

  float corner[8];
  for (uint32_t i = 0; i < 8u; ++i) {
    const glm::uvec3 offset(i & 1u, (i >> 1) & 1u, (i >> 2) & 1u);
    corner[i] = field.distances[latticeIndex(field, cell.base + offset)];
  }

  const float x00 = glm::mix(corner[0], corner[1], cell.t.x);
  const float x10 = glm::mix(corner[2], corner[3], cell.t.x);
  const float x01 = glm::mix(corner[4], corner[5], cell.t.x);
  const float x11 = glm::mix(corner[6], corner[7], cell.t.x);

  const float y0 = glm::mix(x00, x10, cell.t.y);
  const float y1 = glm::mix(x01, x11, cell.t.y);

  return glm::mix(y0, y1, cell.t.z);
}

uint32_t material(const SampledSdf& field, const glm::vec3& point) {
  if (outsideDistance(field, point) > 0.0f) {
    return 0;
  }

  const Cell cell = locate(field, point);

  const glm::uvec3 nearest = cell.base
                             + glm::uvec3(
                               cell.t.x < 0.5f ? 0u : 1u,
                               cell.t.y < 0.5f ? 0u : 1u,
                               cell.t.z < 0.5f ? 0u : 1u
                             );

  return field.materials[latticeIndex(field, nearest)];
}

glm::vec3 gradient(const SampledSdf& field, const glm::vec3& point) {
  const float outside = outsideDistance(field, point);

  if (outside > 0.0f) {
    const glm::vec3 clamped =
      glm::clamp(point, field.origin, maximumCorner(field));

    return (point - clamped) / outside;
  }

  const Cell cell = locate(field, point);

  float c[8];
  readCorners(field, cell.base, c);

  const float u = cell.t.x;
  const float v = cell.t.y;
  const float w = cell.t.z;

  const glm::vec3 derivative(
    (c[1] - c[0]) * (1.0f - v) * (1.0f - w) + (c[3] - c[2]) * v * (1.0f - w)
      + (c[5] - c[4]) * (1.0f - v) * w + (c[7] - c[6]) * v * w,
    (c[2] - c[0]) * (1.0f - u) * (1.0f - w) + (c[3] - c[1]) * u * (1.0f - w)
      + (c[6] - c[4]) * (1.0f - u) * w + (c[7] - c[5]) * u * w,
    (c[4] - c[0]) * (1.0f - u) * (1.0f - v) + (c[5] - c[1]) * u * (1.0f - v)
      + (c[6] - c[2]) * (1.0f - u) * v + (c[7] - c[3]) * u * v
  );

  return derivative / field.voxelSize;
}

FieldProbe probe(const SampledSdf& field, const glm::vec3& point) {
  const glm::vec3 clamped =
    glm::clamp(point, field.origin, maximumCorner(field));

  const glm::vec3 gap = point - clamped;
  const float outside = glm::length(gap);

  const float inner = distance(field, clamped);
  const glm::vec3 derivative = gradient(field, clamped);
  const float steepness = glm::length(derivative);

  const bool usable = steepness > GRADIENT_FLOOR;

  if (outside <= 0.0f) {
    return {
      inner,
      usable ? derivative / steepness : glm::vec3(0.0f, 1.0f, 0.0f)
    };
  }

  const glm::vec3 blended =
    (usable ? derivative / steepness * std::fabs(inner) : glm::vec3(0.0f))
    + gap;

  const float reach = glm::length(blended);

  return {
    inner + outside,
    reach > GRADIENT_FLOOR ? blended / reach : gap / outside
  };
}

float stepBound(
  const SampledSdf& field,
  const glm::vec3& point,
  const glm::vec3& direction
) {
  const float outside = outsideDistance(field, point);

  if (outside > 0.0f) {
    return outside;
  }

  const Cell cell = locate(field, point);
  const glm::uvec3 bricks = brickCounts(field);

  const glm::uvec3 brick =
    glm::min(cell.base / glm::uvec3(BRICK_CELLS), bricks - glm::uvec3(1u));

  glm::vec3 minimum(0.0f);
  glm::vec3 maximum(0.0f);
  brickBox(field, brick, minimum, maximum);

  float exit = std::numeric_limits<float>::max();

  for (int axis = 0; axis < 3; ++axis) {
    if (std::abs(direction[axis]) < 1e-8f) {
      continue;
    }

    const float wall = direction[axis] > 0.0f ? maximum[axis] : minimum[axis];

    exit = std::min(exit, (wall - point[axis]) / direction[axis]);
  }

  const glm::vec3 voxel = field.voxelSize;

  exit = std::max(exit, 0.5f * std::min(voxel.x, std::min(voxel.y, voxel.z)));

  const float bound = field.brickLipschitz[brickIndex(bricks, brick)];

  if (bound <= 0.0f) {
    return exit;
  }

  return std::min(std::abs(distance(field, point)) / bound, exit);
}

glm::uvec3 brickCounts(const SampledSdf& field) {
  return (cellCounts(field) + glm::uvec3(BRICK_CELLS - 1u))
         / glm::uvec3(BRICK_CELLS);
}

bool brickHoldsSurface(const SampledSdf& field, uint32_t brick) {
  return field.brickMinimum[brick] <= 0.0f && field.brickMaximum[brick] >= 0.0f;
}

WriteReport write(
  SampledSdf& field,
  const SampleBox& box,
  std::span<const float> distances,
  std::span<const uint8_t> materials
) {
  const glm::uvec3 end = box.minimum + box.extent;

  if (
    end.x > field.resolution.x || end.y > field.resolution.y
    || end.z > field.resolution.z
  ) {
    throw std::runtime_error("A write must stay inside the lattice");
  }

  const size_t count =
    static_cast<size_t>(box.extent.x) * box.extent.y * box.extent.z;

  if (distances.size() != count || materials.size() != count) {
    throw std::runtime_error(
      "A write needs one distance and one material per sample in the box"
    );
  }

  if (count == 0u) {
    return {box, glm::uvec3(0u), glm::uvec3(0u)};
  }

  size_t source = 0;

  for (uint32_t z = 0; z < box.extent.z; ++z) {
    for (uint32_t y = 0; y < box.extent.y; ++y) {
      for (uint32_t x = 0; x < box.extent.x; ++x) {
        const uint32_t index =
          latticeIndex(field, box.minimum + glm::uvec3(x, y, z));

        field.distances[index] = distances[source];
        field.materials[index] = materials[source];

        ++source;
      }
    }
  }

  const glm::uvec3 cells = cellCounts(field);

  const glm::uvec3 cellMinimum =
    glm::max(box.minimum, glm::uvec3(1u)) - glm::uvec3(1u);
  const glm::uvec3 cellMaximum =
    glm::min(end - glm::uvec3(1u), cells - glm::uvec3(1u));

  const glm::uvec3 reachMinimum =
    glm::max(cellMinimum, glm::uvec3(1u)) - glm::uvec3(1u);
  const glm::uvec3 reachMaximum =
    glm::min(cellMaximum + glm::uvec3(1u), cells - glm::uvec3(1u));

  const BrickRange bricks = rebuildBricks(field, reachMinimum, reachMaximum);

  return {box, bricks.begin, bricks.end};
}

}
