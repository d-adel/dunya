#include "shadowgrid.ih"

namespace dunya::renderer {

namespace {

constexpr uint32_t CELL_COUNT = SHADOW_GRID_CELLS * SHADOW_GRID_CELLS;

glm::vec3 acrossFrom(const glm::vec3& toLight) {
  const glm::vec3 axis = std::abs(toLight.y) < 0.9f
                           ? glm::vec3(0.0f, 1.0f, 0.0f)
                           : glm::vec3(1.0f, 0.0f, 0.0f);

  return glm::normalize(glm::cross(axis, toLight));
}

uint32_t cellOf(float coordinate, float origin, float inverseSize) {
  const float at = (coordinate - origin) * inverseSize;

  if (at <= 0.0f) {
    return 0u;
  }

  const uint32_t cell = static_cast<uint32_t>(at);

  return cell >= SHADOW_GRID_CELLS ? SHADOW_GRID_CELLS - 1u : cell;
}

}  // namespace

void ShadowGrid::giveUp() {
  m_uniform.cell = glm::vec4(0.0f);
  m_cells.clear();
  m_indices.clear();
}

void ShadowGrid::build(
  std::span<const RecordBounds> bounds,
  uint32_t count,
  const glm::vec3& toLight
) {
  if (count == 0u || count > bounds.size()) {
    giveUp();

    return;
  }

  const glm::vec3 axisU = acrossFrom(toLight);
  const glm::vec3 axisV = glm::cross(toLight, axisU);

  m_footprints.clear();
  m_footprints.reserve(count);

  glm::vec2 minimum(std::numeric_limits<float>::max());
  glm::vec2 maximum(std::numeric_limits<float>::lowest());

  for (uint32_t record = 0u; record != count; ++record) {
    const glm::vec3 low(bounds[record].minimum);
    const glm::vec3 high(bounds[record].maximum);

    if (glm::any(glm::greaterThan(low, high))) {
      m_footprints.push_back(glm::vec4(1.0f, 1.0f, -1.0f, -1.0f));

      continue;
    }

    glm::vec2 footprintLow(std::numeric_limits<float>::max());
    glm::vec2 footprintHigh(std::numeric_limits<float>::lowest());

    for (uint32_t corner = 0u; corner != 8u; ++corner) {
      const glm::vec3 at(
        (corner & 1u) != 0u ? high.x : low.x,
        (corner & 2u) != 0u ? high.y : low.y,
        (corner & 4u) != 0u ? high.z : low.z
      );

      const glm::vec2 flat(glm::dot(at, axisU), glm::dot(at, axisV));

      footprintLow = glm::min(footprintLow, flat);
      footprintHigh = glm::max(footprintHigh, flat);
    }

    m_footprints.push_back(
      glm::vec4(
        footprintLow.x,
        footprintLow.y,
        footprintHigh.x,
        footprintHigh.y
      )
    );

    minimum = glm::min(minimum, footprintLow);
    maximum = glm::max(maximum, footprintHigh);
  }

  const glm::vec2 span = maximum - minimum;

  if (span.x <= 0.0f || span.y <= 0.0f) {
    giveUp();

    return;
  }

  const glm::vec2 size = span * 1.001f / float(SHADOW_GRID_CELLS);
  const glm::vec2 inverseSize(1.0f / size.x, 1.0f / size.y);

  m_cells.assign(CELL_COUNT, ShadowCell{});

  for (uint32_t record = 0u; record != count; ++record) {
    const glm::vec4& footprint = m_footprints[record];

    if (footprint.z < footprint.x) {
      continue;
    }

    const uint32_t firstX = cellOf(footprint.x, minimum.x, inverseSize.x);
    const uint32_t lastX = cellOf(footprint.z, minimum.x, inverseSize.x);
    const uint32_t firstY = cellOf(footprint.y, minimum.y, inverseSize.y);
    const uint32_t lastY = cellOf(footprint.w, minimum.y, inverseSize.y);

    for (uint32_t y = firstY; y <= lastY; ++y) {
      for (uint32_t x = firstX; x <= lastX; ++x) {
        ++m_cells[y * SHADOW_GRID_CELLS + x].count;
      }
    }
  }

  uint32_t running = 0u;

  for (ShadowCell& cell : m_cells) {
    cell.offset = running;
    running += cell.count;

    cell.count = 0u;
  }

  if (running > SHADOW_GRID_MAX_INDICES) {
    giveUp();

    return;
  }

  m_indices.assign(running, 0u);

  for (uint32_t record = 0u; record != count; ++record) {
    const glm::vec4& footprint = m_footprints[record];

    if (footprint.z < footprint.x) {
      continue;
    }

    const uint32_t firstX = cellOf(footprint.x, minimum.x, inverseSize.x);
    const uint32_t lastX = cellOf(footprint.z, minimum.x, inverseSize.x);
    const uint32_t firstY = cellOf(footprint.y, minimum.y, inverseSize.y);
    const uint32_t lastY = cellOf(footprint.w, minimum.y, inverseSize.y);

    for (uint32_t y = firstY; y <= lastY; ++y) {
      for (uint32_t x = firstX; x <= lastX; ++x) {
        ShadowCell& cell = m_cells[y * SHADOW_GRID_CELLS + x];

        m_indices[cell.offset + cell.count] = record;
        ++cell.count;
      }
    }
  }

  m_uniform.axisU = glm::vec4(axisU, minimum.x);
  m_uniform.axisV = glm::vec4(axisV, minimum.y);
  m_uniform.cell = glm::vec4(
    inverseSize.x,
    inverseSize.y,
    float(SHADOW_GRID_CELLS),
    float(SHADOW_GRID_CELLS)
  );
}

const ShadowGridUniform& ShadowGrid::uniform() const noexcept {
  return m_uniform;
}

std::span<const ShadowCell> ShadowGrid::cells() const noexcept {
  return m_cells;
}

std::span<const uint32_t> ShadowGrid::indices() const noexcept {
  return m_indices;
}

}  // namespace dunya::renderer
