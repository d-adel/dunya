#pragma once

// Named by what is being compared, not by magnitude. A sampled representation
// will need its own, and one shared constant would have to be wrong for
// somebody.
constexpr float ANALYTIC_TOLERANCE = 1e-5f;
constexpr float GRADIENT_TOLERANCE = 1e-3f;

// A march stops within its own epsilon of the surface, so a hit position is
// only ever that accurate.
constexpr float MARCH_TOLERANCE = 1e-2f;
