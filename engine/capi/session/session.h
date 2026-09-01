#pragma once

#include <dunya/engine/engine/engine.h>
#include <dunya/viewport/camera/camera.h>

#include <dunya/gpu/windowsystem/windowsystem.h>
#include <dunya/field/field.h>
#include <dunya/objectmodel/component/pose/pose.h>
#include <dunya/objectmodel/component/sdfgrid/sdfgrid.h>
#include <dunya/renderer/scenetarget/scenetarget.h>
#include <dunya/viewport/grid/grid.h>

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
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

  void showGrid(bool visible) noexcept;

  [[nodiscard]] bool gridVisible() const noexcept;

  void drawGrid(VkCommandBuffer commands) const;

  void drawSky(VkCommandBuffer commands) const;

  void setSupersample(float scale);

  [[nodiscard]] float supersample() const noexcept;

  [[nodiscard]] size_t materialCount() const noexcept;

  [[nodiscard]] dunya::core::AssetId materialAt(uint32_t index) const noexcept;

  [[nodiscard]] uint32_t materialIndex(dunya::core::AssetId id) const noexcept;

  [[nodiscard]] dunya::core::AssetId addMaterial(
    const glm::vec4& baseColor,
    float metallic,
    float roughness
  );

private:
  void lookAtWorld(float aspect);

  void frameCameraOnWorld();

  dunya::engine::Engine m_engine;

  std::string m_worldName;
  dunya::viewport::Camera m_camera;

  bool m_viewsThroughScene = false;

  dunya::renderer::SceneTarget m_sceneTarget;

  dunya::viewport::Grid m_grid;

  bool m_gridVisible = true;
};

}
