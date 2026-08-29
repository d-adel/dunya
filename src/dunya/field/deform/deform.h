#pragma once

#include <dunya/field/field.h>
#include <dunya/field/sampled/sampled.h>

#include <glm/glm.hpp>

#include <cstdint>

namespace dunya::field {

// How many voxels past a primitive own bound a deformation still has to be
// written - counted per axis, not as one world distance. A grid whose axes
// differ by ten has no single "8 voxels": taking the coarsest made the box
// 84 voxels deep on the fine axis and cost 140 ms a dent.
// A CSG operator changes a value wherever the new surface beats the old one,
// and that reaches arbitrarily far - a point five metres inside a wall really
// is 2.65 from a hole 3 away, and a welded blob is nearer to the exterior than
// whatever was there before. Past this margin the only values it would change
// are already deeper than the band D7 promises to redistance, so stopping here
// is what the invariant allows rather than an approximation.
inline constexpr uint32_t DEFORM_BAND_VOXELS = 4u;

// The lattice points a primitive can change, clamped to the grid. Zero extent
// when it misses the grid entirely, which is begin == end and needs no
// sentinel. The primitive's own cull sphere is the bound, so this is
// conservative for every shape and costs nothing to maintain.
SampleBox affectedBox(
  const SampledField& field,
  const Primitive& primitive,
  uint32_t marginVoxels
);

// Folds one primitive into the stored lattice under its own operation - carve,
// weld, blend, intersect - in the field's own space. The same fold sample()
// runs over a walk, applied to a grid instead, so a deformation and a bake
// cannot disagree about what subtraction means.
//
// Exact about the sign and the zero set. Outside the solid a hard subtraction
// underestimates, which is safe for a march and slow for one, and is what the
// redistance sweep exists to repair.
WriteReport deform(SampledField& field, const Primitive& primitive);

// What a deformation did, and whether its repair settled. `converged` is the
// largest change the last sweep still made, so zero means another sweep would
// change nothing. Deliberately not a ||grad phi| - 1| residual: that is a kink
// measurement, and a crater has a medial axis inside the band it would be
// taken over.
struct DeformReport {
  WriteReport write;
  float converged = 0.0f;
};

// Folds the primitive in and repairs the band it damaged, in one call.
//
// One call because only the fold knows which points it changed, and a sweep
// over a point it did not touch makes the field worse rather than better: a
// sweep is first order, so on the ground's own lattice - axes a factor of ten
// apart - redistancing an undamaged field moved the surface by nine fine
// voxels of visible lumps. Splitting these would mean guessing that set back
// out of the values, which cannot be done exactly.
DeformReport deformAndRepair(
  SampledField& field,
  const Primitive& primitive,
  uint32_t sweeps = 8u
);

}  // namespace dunya::field
