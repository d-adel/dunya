#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace dunya::editor {

struct PackageSpec {
  std::filesystem::path runtimeExecutable;
  std::filesystem::path projectRoot;
  std::filesystem::path output;

  std::vector<std::string> worlds;
};

[[nodiscard]] std::filesystem::path packagedExecutable(const PackageSpec& spec);

[[nodiscard]] bool packageProject(const PackageSpec& spec, std::string& failure);

}
