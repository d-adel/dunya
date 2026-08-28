#pragma once

#include <cstdint>
#include <span>
#include <string>

// What the process was asked to do before the first frame. A measurement
// harness: every option makes a comparison repeatable, none changes the render.
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
