#pragma once

#include <dunya/field/capability/distancefield.h>
#include <dunya/field/capability/gradientquery.h>
#include <dunya/field/capability/materialquery.h>
#include <dunya/field/capability/stepbound.h>
#include <dunya/field/field.h>

#include <glm/glm.hpp>

#include <cstdint>
#include <span>
#include <vector>

namespace dunya::field {

inline constexpr uint32_t BRICK_CELLS = DUNYA_BRICK_CELLS;

struct SampledField {
  glm::vec3 origin{0.0f};
  glm::vec3 voxelSize{1.0f};
  glm::uvec3 resolution{0u};

  std::vector<float> distances;
  std::vector<uint8_t> materials;

  std::vector<float> brickLipschitz;
  float globalLipschitz = 0.0f;

  std::vector<float> brickMinimum;
  std::vector<float> brickMaximum;
};

struct SampleBox {
  glm::uvec3 minimum{0u};
  glm::uvec3 extent{0u};
};

SampleBox merge(const SampleBox& first, const SampleBox& second);

struct WriteReport {
  SampleBox samples;
  glm::uvec3 brickBegin{0u};
  glm::uvec3 brickEnd{0u};
};

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

float distance(const SampledField& field, const glm::vec3& point);

uint32_t material(const SampledField& field, const glm::vec3& point);

glm::vec3 gradient(const SampledField& field, const glm::vec3& point);

struct FieldProbe {
  float distance = 0.0f;
  glm::vec3 normal{0.0f, 1.0f, 0.0f};
};

FieldProbe probe(const SampledField& field, const glm::vec3& point);
float stepBound(
  const SampledField& field,
  const glm::vec3& point,
  const glm::vec3& direction
);

glm::uvec3 brickCounts(const SampledField& field);
bool brickHoldsSurface(const SampledField& field, uint32_t brick);

float bakeError(const SampledField& field, float sourceLipschitz = 1.0f);

WriteReport write(
  SampledField& field,
  const SampleBox& box,
  std::span<const float> distances,
  std::span<const uint8_t> materials
);

static_assert(DistanceField<SampledField>);
static_assert(MaterialQueryable<SampledField>);
static_assert(GradientQueryable<SampledField>);
static_assert(StepBounded<SampledField>);

}  // namespace dunya::field
