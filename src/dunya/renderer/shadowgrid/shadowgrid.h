#pragma once

#include <dunya/renderer/fieldrecord/fieldrecord.h>

#include <glm/glm.hpp>

#include <cstdint>
#include <span>
#include <vector>

namespace dunya::renderer {

inline constexpr uint32_t SHADOW_GRID_CELLS = 64u;

inline constexpr uint32_t SHADOW_GRID_MAX_INDICES = 1u << 17;

struct ShadowGridUniform {
  glm::vec4 axisU{1.0f, 0.0f, 0.0f, 0.0f};
  glm::vec4 axisV{0.0f, 0.0f, 1.0f, 0.0f};
  glm::vec4 cell{0.0f};
};

static_assert(
  sizeof(ShadowGridUniform) == 48,
  "ShadowGridUniform must match its std140 block in field-shader.frag"
);

struct ShadowCell {
  uint32_t offset = 0u;
  uint32_t count = 0u;
};

static_assert(sizeof(ShadowCell) == 8, "ShadowCell is read as a uvec2");

class ShadowGrid {
public:
  void build(
    std::span<const RecordBounds> bounds,
    uint32_t count,
    const glm::vec3& toLight
  );

  [[nodiscard]] const ShadowGridUniform& uniform() const noexcept;
  [[nodiscard]] std::span<const ShadowCell> cells() const noexcept;
  [[nodiscard]] std::span<const uint32_t> indices() const noexcept;

private:
  void giveUp();

  ShadowGridUniform m_uniform{};
  std::vector<ShadowCell> m_cells;
  std::vector<uint32_t> m_indices;

  std::vector<glm::vec4> m_footprints;
};

}  // namespace dunya::renderer
