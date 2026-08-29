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

  // Dents the scene's deformable this many times before the first frame, at
  // the same positions every run. This is the measurement M30 turns on.
  uint32_t dents = 0;

  // One row per dent: what each stage cost, how many bricks it touched, and
  // how much the object is holding. Empty means measure nothing.
  std::string dentLog;

  // Plays the scene for this many frames, firing on a fixed schedule, then
  // exits. The acceptance run for impact deformation: a ball is thrown by the
  // same key a person would press, and what it craters is reported.
  uint32_t demo = 0;

  // Shots per second while a demo run is playing. Zero keeps the default.
  float demoRate = 0.0f;

  // The wall, in boxes. Every one is its own field with its own volume, so
  // this is the knob that says how much the renderer is being asked to do.
  uint32_t wallColumns = 4;
  uint32_t wallRows = 5;

  // Layers deep. One is a screen that falls over on the first hit; a wall
  // worth shooting at has to be chewed through.
  uint32_t wallDepth = 1;

  // Writes every presented frame to this directory as a numbered PNG. For
  // recording rather than for testing, so it does not exit after one.
  std::string capture;

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
