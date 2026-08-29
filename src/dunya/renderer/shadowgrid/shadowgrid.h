#pragma once

#include <dunya/renderer/fieldrecord/fieldrecord.h>

#include <glm/glm.hpp>

#include <cstdint>
#include <span>
#include <vector>

namespace dunya::renderer {

// Cells per axis across the light. Sixty-four puts a crate in a handful of
// cells at the scales this scene runs at, and the table is 32 KB either way.
inline constexpr uint32_t SHADOW_GRID_CELLS = 64u;

// The flat list of record indices, capped. Overflow is not an error and not a
// truncation: the grid reports itself unbuilt and the shader walks every
// record, which is what it did before this existed.
inline constexpr uint32_t SHADOW_GRID_MAX_INDICES = 1u << 17;

// The basis and the extent, as the fragment shader reads them.
//
// axisU.xyz, axisV.xyz: an orthonormal pair spanning the plane across the
// light. axisU.w, axisV.w: where the grid starts on each.
// cell.xy: reciprocal cell size. cell.zw: cells per axis, zero meaning the
// grid is not built and every record has to be walked.
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

// A shadow ray travels along one fixed direction, so its position across the
// light never changes: a record can shadow a point only if the point falls
// inside the record's own footprint on the plane across the light. Binning the
// records by that footprint turns the per-pixel walk from every record into
// the ones that can be overhead.
//
// Conservative by construction - the footprint comes from the record's world
// box, and a box contains what it bounds - so the slab test behind it still
// decides, and this only decides who gets asked.
class ShadowGrid {
public:
  // Rebuilt per frame from the same boxes the slab test reads. `count` is the
  // live record count, not the table's capacity.
  void build(
    std::span<const RecordBounds> bounds,
    uint32_t count,
    const glm::vec3& toLight
  );

  [[nodiscard]] const ShadowGridUniform& uniform() const noexcept;
  [[nodiscard]] std::span<const ShadowCell> cells() const noexcept;
  [[nodiscard]] std::span<const uint32_t> indices() const noexcept;

private:
  // Says the grid holds nothing and every record must be walked.
  void giveUp();

  ShadowGridUniform m_uniform{};
  std::vector<ShadowCell> m_cells;
  std::vector<uint32_t> m_indices;

  // Reused across frames so a per-frame rebuild allocates nothing.
  std::vector<glm::vec4> m_footprints;
};

}  // namespace dunya::renderer
