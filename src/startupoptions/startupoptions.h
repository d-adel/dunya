#pragma once

#include <cstdint>
#include <span>
#include <string>

/* What the process was asked to do before the first frame.
 *
 * A measurement harness, not a feature. Reproducing a comparison means running
 * the same scene at the same primitive count in each representation, and a hand
 * on the keyboard reproduces neither between runs. Every option here exists to
 * make a measurement repeatable; none of them changes what the renderer is.
 */
struct StartupOptions {
  StartupOptions() = default;
  explicit StartupOptions(std::span<char*> arguments);

  // Carves this many spheres before the first frame, at fixed positions.
  uint32_t carves = 0;

  // Falls back to the exact field. M17 chose the sampled one, so this asks for
  // the reference rather than for a feature.
  bool analytic = false;

  // Compares the compute bake against a CPU bake of the same primitives.
  bool verifyBake = false;

  // Writes the first presented frame here as a PNG and exits. Empty means run
  // normally.
  std::string screenshot;

  // Compares the first presented frame against this reference and exits with a
  // failing status if it has drifted. Empty means run normally.
  std::string golden;
};
