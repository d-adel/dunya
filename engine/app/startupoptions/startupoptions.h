#pragma once

#include <cstdint>
#include <span>
#include <string>

struct StartupOptions {
  StartupOptions() = default;
  explicit StartupOptions(std::span<char*> arguments);

  std::string capture;

  bool analytic = false;

  bool verifyBake = false;

  std::string screenshot;

  std::string golden;

  std::string project = "projects/demo";

  std::string world = "main";

  bool packaged = false;
};
