#pragma once

#include <dunya/field/field.h>
#include <dunya/field/sampled/sampled.h>

#include <glm/glm.hpp>

#include <cstdint>

namespace dunya::field {

inline constexpr uint32_t DEFORM_BAND_VOXELS = 4u;

SampleBox affectedBox(
  const SampledField& field,
  const Primitive& primitive,
  uint32_t marginVoxels
);

WriteReport deform(SampledField& field, const Primitive& primitive);

struct DeformReport {
  WriteReport write;
  float converged = 0.0f;
};

DeformReport deformAndRepair(
  SampledField& field,
  const Primitive& primitive,
  uint32_t sweeps = 8u
);

}
