#pragma once

#include <dunya/assets/assetlibrary/assetlibrary.h>
#include <dunya/core/telemetry/telemetry.h>
#include <dunya/gpu/context/context.h>
#include <dunya/gpu/pipeline/pipeline.h>
#include <dunya/gpu/swapchain/swapchain.h>
#include <dunya/gpu/windowsystem/windowsystem.h>
#include <dunya/objectmodel/world/world.h>
#include <dunya/physics/joltlibrary/joltlibrary.h>
#include <dunya/renderer/frame/frame.h>
#include <dunya/renderer/renderer.h>
#include <dunya/renderer/rendererstorage/rendererstorage.h>
#include <dunya/runtime/runtime/runtime.h>
#include <dunya/script/api/api.h>
#include <dunya/systems/input/input.h>
#include <dunya/systems/schedule/schedule.h>
#include <dunya/view/lookthrough/lookthrough.h>
#include <dunya/view/viewportstore/viewportstore.h>

#include <filesystem>
#include <functional>
#include <memory>
#include <span>
#include <optional>
#include <string>
#include <vector>

namespace dunya::engine {

struct FrameRequest {
  dunya::view::ViewportId viewport = dunya::view::INVALID_VIEWPORT;

  float deltaSeconds = 0.0f;

  bool surfaceStale = false;

  std::span<const dunya::renderer::ScenePass> passes = {};

  std::function<void(VkImage)> capture{};
};

class Engine {
public:
  Engine(
    std::unique_ptr<dunya::gpu::WindowSystem> windowSystem,
    const std::filesystem::path& projectRoot
  );
  ~Engine();

  Engine(const Engine&) = delete;
  Engine& operator=(const Engine&) = delete;
  Engine(Engine&&) = delete;
  Engine& operator=(Engine&&) = delete;

  void loadWorld(const std::string& world);

  [[nodiscard]] dunya::view::ViewportId createViewport();

  [[nodiscard]] bool destroyViewport(dunya::view::ViewportId id);

  [[nodiscard]] bool configureViewport(
    dunya::view::ViewportId id,
    const dunya::view::Viewport& config
  );

  [[nodiscard]] const dunya::view::Viewport* viewport(
    dunya::view::ViewportId id
  ) const;

  [[nodiscard]] const std::filesystem::path& projectRoot() const noexcept;

  void play();

  void stop();

  [[nodiscard]] bool playing() const noexcept;

  [[nodiscard]] dunya::objectmodel::World& world() noexcept;
  [[nodiscard]] const dunya::objectmodel::World& world() const noexcept;

  [[nodiscard]] dunya::objectmodel::World& activeWorld() noexcept;
  [[nodiscard]] const dunya::objectmodel::World& activeWorld() const noexcept;

  [[nodiscard]] dunya::assets::AssetLibrary& assets() noexcept;
  [[nodiscard]] const dunya::assets::AssetLibrary& assets() const noexcept;

  [[nodiscard]] dunya::gpu::Context& context() noexcept;
  [[nodiscard]] const dunya::gpu::Context& context() const noexcept;

  [[nodiscard]] dunya::gpu::SwapChain& swapChain() noexcept;
  [[nodiscard]] const dunya::gpu::SwapChain& swapChain() const noexcept;

  [[nodiscard]] dunya::gpu::WindowSystem& windowSystem() noexcept;

  [[nodiscard]] dunya::renderer::RendererStorage& storage() noexcept;
  [[nodiscard]] const dunya::renderer::RendererStorage&
  storage() const noexcept;

  [[nodiscard]] dunya::renderer::Renderer& renderer() noexcept;
  [[nodiscard]] const dunya::renderer::Renderer& renderer() const noexcept;

  [[nodiscard]] dunya::systems::Schedule& schedule() noexcept;

  [[nodiscard]] dunya::systems::InputState& input() noexcept;
  [[nodiscard]] const dunya::systems::InputState& input() const noexcept;

  [[nodiscard]] dunya::runtime::Runtime* runtime() noexcept;

  [[nodiscard]] uint32_t frameIndex() const noexcept;

  [[nodiscard]] bool renderFrame(
    const FrameRequest& request,
    dunya::core::Telemetry& telemetry
  );

  [[nodiscard]] VkDescriptorSet globals() const noexcept;

  void resize();

  void retarget(std::unique_ptr<dunya::gpu::WindowSystem> windowSystem);

  [[nodiscard]] std::vector<VkDescriptorSetLayout> setLayouts(
    dunya::gpu::PipelineType type
  );

private:
  static int32_t onSetRigidBody(
    void* host,
    void* world,
    uint32_t entity,
    float mass
  );

  static int32_t onSetVelocity(
    void* host,
    void* world,
    uint32_t entity,
    const float* velocity
  );

  static int32_t onDestroy(void* host, void* world, uint32_t entity);

  static const dunya::script::PhysicsVerbs PHYSICS_VERBS;

  struct PendingBody {
    dunya::objectmodel::Entity entity;
    std::optional<float> mass;
    std::optional<glm::vec3> velocity;
  };

  PendingBody& remember(dunya::objectmodel::Entity entity);

  void applyPendingBodies();

  void lookThroughViewport(dunya::view::ViewportId id, float aspect);

  void tick(float deltaSeconds, dunya::core::Telemetry& telemetry);

  void flushVolumes(dunya::core::Telemetry& telemetry);

  void packFrame(bool holdBakes = false);

  void endFrame() noexcept;

  void drawSky(VkCommandBuffer commands) const;

  void stepPhysics(float deltaSeconds, dunya::core::Telemetry& telemetry);

  void runSystems(float deltaSeconds);

  std::unique_ptr<dunya::gpu::WindowSystem> m_windowSystem;
  dunya::gpu::Context m_context;
  dunya::gpu::SwapChain m_swapChain;

  std::filesystem::path m_projectRoot;

  dunya::assets::AssetLibrary m_assetLibrary;

  dunya::objectmodel::World m_world;

  dunya::physics::JoltLibrary m_jolt;
  std::optional<dunya::runtime::Runtime> m_runtime;

  dunya::script::PhysicsScope m_physicsScope;

  std::vector<PendingBody> m_pendingBodies;

  dunya::systems::Schedule m_schedule;
  dunya::systems::InputState m_input;

  dunya::view::ViewportStore m_viewports;

  uint32_t m_frameIndex = 0;

  double m_accumulator = 0.0;

  dunya::renderer::RendererStorage m_storage;

  dunya::gpu::Pipeline m_meshPipeline;
  dunya::gpu::Pipeline m_sdfPipeline;
  dunya::gpu::Pipeline m_skyPipeline;

  dunya::renderer::Renderer m_renderer;

  dunya::renderer::Frame m_frame{};
};

}
