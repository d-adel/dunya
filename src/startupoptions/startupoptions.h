#pragma once

#include <cstdint>
#include <span>
#include <string>

struct StartupOptions {
  StartupOptions() = default;
  explicit StartupOptions(std::span<char*> arguments);

  uint32_t carves = 0;

  uint32_t dents = 0;

  std::string dentLog;

  uint32_t demo = 0;

  float demoRate = 0.0f;

  uint32_t wallColumns = 4;
  uint32_t wallRows = 5;

  uint32_t wallDepth = 1;

  std::string capture;

  bool analytic = false;

  bool verifyBake = false;

  std::string screenshot;

  std::string golden;
};
