#pragma once

#include <dunya/core/asset/asset.h>
#include <dunya/serialize/worldfile/worldfile.h>

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace dunya::serialize {

inline constexpr uint32_t PROJECT_VERSION = 1u;

inline constexpr std::string_view MANIFEST_NAME = "project.dunya";
inline constexpr std::string_view ASSET_FOLDER = "assets";
inline constexpr std::string_view WORLD_FOLDER = "worlds";

struct AssetEntry {
  dunya::core::AssetId id = dunya::core::INVALID_ASSET;
  std::string type;
  std::string path;
};

struct ProjectManifest {
  uint32_t version = PROJECT_VERSION;
  std::string name;
  std::vector<AssetEntry> assets;
};

class Project {
public:
  [[nodiscard]] static std::optional<Project> open(
    const std::filesystem::path& root
  );

  [[nodiscard]] static std::optional<Project> create(
    const std::filesystem::path& root,
    std::string name
  );

  [[nodiscard]] bool save() const;

  [[nodiscard]] const ProjectManifest& manifest() const noexcept;

  [[nodiscard]] const std::filesystem::path& root() const noexcept;

  [[nodiscard]] dunya::core::AssetId importAsset(
    const std::filesystem::path& file,
    std::string type
  );

  [[nodiscard]] const AssetEntry* find(dunya::core::AssetId id) const noexcept;

  [[nodiscard]] std::vector<const AssetEntry*> ofType(
    std::string_view type
  ) const;

  [[nodiscard]] bool exportTo(
    const std::filesystem::path& root,
    std::span<const dunya::core::AssetId> wanted
  ) const;

  [[nodiscard]] bool saveWorld(
    std::string_view name,
    const StoredWorld& stored
  ) const;

  [[nodiscard]] bool loadWorld(
    std::string_view name,
    StoredWorld& stored
  ) const;

private:
  Project(std::filesystem::path root, ProjectManifest manifest);

  std::filesystem::path m_root;
  ProjectManifest m_manifest;
};

[[nodiscard]] dunya::core::AssetId mintAssetId();

}
