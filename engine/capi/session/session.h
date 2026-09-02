#pragma once

#include <dunya/engine/engine/engine.h>
#include <dunya/view/camera/camera.h>

#include <dunya/gpu/windowsystem/windowsystem.h>
#include <dunya/field/field.h>
#include <dunya/objectmodel/component/pose/pose.h>
#include <dunya/objectmodel/component/sdfgrid/sdfgrid.h>
#include <dunya/renderer/scenetarget/scenetarget.h>
#include <dunya/gizmos/grid/grid.h>
#include <dunya/undo/undostack/undostack.h>

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace dunya::capi {

class Session {
public:
  Session(
    std::unique_ptr<dunya::gpu::WindowSystem> windowSystem,
    const std::filesystem::path& projectRoot,
    const std::string& world
  );
  ~Session();

  Session(const Session&) = delete;
  Session& operator=(const Session&) = delete;
  Session(Session&&) = delete;
  Session& operator=(Session&&) = delete;

  void resize();

  void retarget(std::unique_ptr<dunya::gpu::WindowSystem> windowSystem);

  void render();

  [[nodiscard]] VkExtent2D extent() const noexcept;

  [[nodiscard]] const dunya::objectmodel::World& world() const noexcept;

  [[nodiscard]] dunya::objectmodel::World& world() noexcept;

  [[nodiscard]] dunya::systems::Schedule& schedule() noexcept;

  void play();

  void stop();

  [[nodiscard]] bool playing() const noexcept;

  [[nodiscard]] dunya::objectmodel::World& activeWorld() noexcept;

  [[nodiscard]] const dunya::objectmodel::World& activeWorld() const noexcept;

  void orbitCamera(float deltaYaw, float deltaPitch);

  void panCamera(float deltaX, float deltaY);

  void zoomCamera(float delta);

  void focusCamera(dunya::objectmodel::Entity entity);

  void alignToSceneCamera();

  [[nodiscard]] bool viewsThroughScene() const noexcept;

  void setKey(uint32_t key, bool down) noexcept;

  void setMouseButton(uint32_t button, bool down) noexcept;

  void setCursor(float x, float y) noexcept;

  [[nodiscard]] bool package(
    const std::string& playerExecutable,
    const std::string& output,
    const std::vector<std::string>& worlds,
    std::string& result
  ) const;

  [[nodiscard]] dunya::objectmodel::Entity pick(float x, float y);

  [[nodiscard]] dunya::objectmodel::Entity createLight();

  [[nodiscard]] dunya::objectmodel::Entity createEnvironment();

  [[nodiscard]] dunya::objectmodel::Entity createCamera(
    const glm::vec3& position,
    const glm::vec3& target,
    float verticalFov
  );

  [[nodiscard]] dunya::objectmodel::Entity createSdf(
    const dunya::objectmodel::Pose& pose,
    const glm::uvec3& resolution,
    float margin
  );

  [[nodiscard]] bool addPrimitive(
    dunya::objectmodel::Entity entity,
    const dunya::field::Primitive& primitive
  );

  void setStatic(dunya::objectmodel::Entity entity);
  void setDeformable(dunya::objectmodel::Entity entity);

  [[nodiscard]] bool destroyEntity(dunya::objectmodel::Entity entity);

  void record(std::string label);

  [[nodiscard]] bool undo();

  [[nodiscard]] bool redo();

  [[nodiscard]] std::optional<std::string_view> undoLabel() const noexcept;

  [[nodiscard]] std::optional<std::string_view> redoLabel() const noexcept;

  [[nodiscard]] bool save() const;

  [[nodiscard]] bool openWorld(const std::string& name);

  [[nodiscard]] bool newWorld(const std::string& name);

  [[nodiscard]] bool saveAs(const std::string& name);

  [[nodiscard]] std::string worldNames() const;

  [[nodiscard]] std::string assetLines() const;

  [[nodiscard]] dunya::core::AssetId importAsset(
    const std::filesystem::path& file,
    const std::string& type
  );

  [[nodiscard]] const std::string& worldName() const noexcept;

  [[nodiscard]] dunya::view::Viewport viewSettings() const noexcept;

  void setViewSettings(const dunya::view::Viewport& settings);

  void drawGrid(VkCommandBuffer commands) const;

  [[nodiscard]] size_t materialCount() const noexcept;

  [[nodiscard]] dunya::core::AssetId materialAt(uint32_t index) const noexcept;

  [[nodiscard]] uint32_t materialIndex(dunya::core::AssetId id) const noexcept;

  [[nodiscard]] dunya::core::AssetId addMaterial(
    const glm::vec4& baseColor,
    float metallic,
    float roughness
  );

private:
  static constexpr size_t UNDO_DEPTH = 32;

  void bindCamera();

  void frameCameraOnWorld();

  dunya::engine::Engine m_engine;

  dunya::undo::UndoStack m_history{UNDO_DEPTH};

  std::string m_worldName;
  dunya::view::Camera m_camera;

  dunya::view::ViewportId m_viewport = dunya::view::INVALID_VIEWPORT;
  dunya::view::Viewport m_port{};

  bool m_viewsThroughScene = false;

  dunya::renderer::SceneTarget m_sceneTarget;

  dunya::gizmos::Grid m_grid;

  std::chrono::steady_clock::time_point m_lastFrame =
    std::chrono::steady_clock::now();
};

}
