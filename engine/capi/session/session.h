#pragma once

#include <dunya/assets/assetlibrary/assetlibrary.h>
#include <dunya/viewport/camera/camera.h>

#include <dunya/gpu/context/context.h>
#include <dunya/gpu/pipeline/pipeline.h>
#include <dunya/gpu/swapchain/swapchain.h>
#include <dunya/gpu/uploader/uploader.h>
#include <dunya/gpu/windowsystem/windowsystem.h>
#include <dunya/objectmodel/world/world.h>
#include <dunya/renderer/frame/frame.h>
#include <dunya/renderer/frameglobals/frameglobals.h>
#include <dunya/field/field.h>
#include <dunya/objectmodel/component/deformable/deformable.h>
#include <dunya/objectmodel/component/pose/pose.h>
#include <dunya/objectmodel/component/material/material.h>
#include <dunya/objectmodel/component/sdfgrid/sdfgrid.h>
#include <dunya/renderer/framepacker/framepacker.h>
#include <dunya/renderer/rendererstorage/rendererstorage.h>
#include <dunya/renderer/renderer.h>
#include <dunya/renderer/resourcetable/resourcetable.h>
#include <dunya/renderer/scenetarget/scenetarget.h>
#include <dunya/viewport/grid/grid.h>
#include <dunya/renderer/sdfbaker/sdfbaker.h>
#include <dunya/renderer/sdfrecordtable/sdfrecordtable.h>
#include <dunya/renderer/sdfresidency/sdfresidency.h>
#include <dunya/physics/joltlibrary/joltlibrary.h>
#include <dunya/renderer/volumepool/volumepool.h>
#include <dunya/runtime/runtime/runtime.h>
#include <dunya/script/api/api.h>
#include <dunya/systems/schedule/schedule.h>

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

  static void onDeform(
    void* host,
    uint32_t entity,
    const dunya::script::SdfDeformSummary* summary
  );
  void stop();

  [[nodiscard]] bool playing() const noexcept;

  [[nodiscard]] dunya::objectmodel::World& activeWorld() noexcept;

  [[nodiscard]] const dunya::objectmodel::World& activeWorld() const noexcept;

  void orbitCamera(float deltaYaw, float deltaPitch);

  void panCamera(float deltaX, float deltaY);

  void zoomCamera(float delta);

  void focusCamera(dunya::objectmodel::Entity entity);

  void alignToSceneCamera();

  [[nodiscard]] dunya::objectmodel::Entity pick(float x, float y);

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
  void loadWorld(
    const std::filesystem::path& projectRoot,
    const std::string& world
  );

  void lookAtWorld(float aspect);

  void frameCameraOnWorld();

  std::unique_ptr<dunya::gpu::WindowSystem> m_windowSystem;
  dunya::gpu::Context m_context;
  dunya::gpu::SwapChain m_swapChain;

  dunya::assets::AssetLibrary m_assetLibrary;

  std::filesystem::path m_projectRoot;
  std::string m_worldName;
  dunya::objectmodel::World m_world;

  dunya::physics::JoltLibrary m_jolt;
  std::optional<dunya::runtime::Runtime> m_runtime;

  dunya::script::SdfDeformScope m_deformScope;

  dunya::systems::Schedule m_schedule;

  dunya::viewport::Camera m_camera;

  uint32_t m_frameIndex = 0;

  dunya::renderer::RendererStorage m_storage;

  dunya::gpu::Pipeline m_meshPipeline;
  dunya::gpu::Pipeline m_sdfPipeline;
  dunya::gpu::Pipeline m_gridPipeline;
  dunya::gpu::Pipeline m_skyPipeline;

  dunya::renderer::SceneTarget m_sceneTarget;

  dunya::viewport::Grid m_grid;

  bool m_gridVisible = true;
  dunya::renderer::Renderer m_renderer;

  dunya::renderer::Frame m_frame;
};

}
