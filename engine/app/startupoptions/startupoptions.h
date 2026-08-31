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

  std::string capture;

  bool analytic = false;

  bool grid = false;

  bool verifyBake = false;

  std::string screenshot;

  std::string golden;

  std::string project = "projects/demo";

  std::string world = "main";

  std::string exportProject;
};
