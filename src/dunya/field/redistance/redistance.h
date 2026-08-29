#pragma once

#include <dunya/field/sampled/sampled.h>

#include <glm/glm.hpp>

#include <cstdint>
#include <span>

namespace dunya::field {

// Solves |grad phi| = 1 inside the box, holding the zero set where it is.
//
// A CSG fold is exact about the sign and the zero set and wrong about
// magnitude away from them, worst at the crease it just made. Nothing breaks:
// the result still under-estimates, so a march cannot overshoot - it just
// takes shorter steps than it needed to, everywhere, for the rest of the run.
// That is what this repairs.
//
// Fast sweeping rather than fast marching: the characteristics of the eikonal
// equation are straight lines, so each of the eight corner-to-corner orderings
// captures one octant of them and a fixed eight sweeps converge. No queue, no
// comparator, and the access pattern is a nested loop over a box, which is
// what a changed region already is.
//
// The box's outer shell is held at the values it arrives with, so the repaired
// interior agrees with the untouched field around it instead of contradicting
// it at the seam.
//
// Returns how much the last sweep still moved anything - zero means another
// sweep would change nothing. Deliberately not a ||grad phi| - 1| residual:
// that is a kink measurement, and a crater has a medial axis inside the band,
// so zero sweeps and four sweeps both scored exactly 1.0 while the converged
// answer was 0.80.
float redistance(
  SampledField& field,
  const SampleBox& box,
  std::span<const uint8_t> damaged,
  std::span<float> values,
  uint32_t sweeps = 8u
);

// Reads and writes the field itself. The overload above exists so a fold and
// its repair can share one write.
float redistance(
  SampledField& field,
  const SampleBox& box,
  std::span<const uint8_t> damaged,
  uint32_t sweeps = 8u
);

// Repairs the whole box. Only right when the whole box is known to be damaged
// - a test that flattened it, say. A sweep is first order, so over a region
// that was already correct it is the thing doing the damage.
float redistance(SampledField& field, const SampleBox& box);

// One Godunov upwind update at a point whose three axis-minimum neighbours are
// (a, b, c) with spacings h. Separated out because it is pure arithmetic with
// no grid in it, so it can be checked against hand-computed cases.
//
// Returns the smallest x solving the equation below. Not necessarily above
// max(a, b, c): a neighbour further than one upwind step drops out entirely,
// which is the whole point of the scheme.
//   [max(x-a,0)/hx]^2 + [max(x-b,0)/hy]^2 + [max(x-c,0)/hz]^2 = 1
float godunov(float a, float b, float c, const glm::vec3& h);

}  // namespace dunya::field
