#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>

#include <dunya/physics/impact/impact.h>
#include <dunya/physics/joltlibrary/joltlibrary.h>
#include <dunya/physics/physicsworld/physicsworld.h>
#include <dunya/runtime/deformation/deformation.h>
#include <dunya/runtime/runtime/runtime.h>
#include <dunya/gpu/context/context.h>
#include <dunya/gpu/swapchain/swapchain.h>
#include <dunya/gpu/uploader/uploader.h>
#include <app/projectile/projectile.h>
#include <dunya/objectmodel/worldquery/worldquery.h>
#include <dunya/renderer/renderer.h>
#include <app/flycamera/flycamera.h>
#include <app/glfwwindowsystem/glfwwindowsystem.h>
#include <dunya/platform/glfwlibrary/glfwlibrary.h>
#include <dunya/platform/input/input.h>
#include <dunya/platform/window/window.h>
#include <dunya/renderer/frame/frame.h>
#include <dunya/renderer/resourcetable/resourcetable.h>
#include <dunya/renderer/frameglobals/frameglobals.h>
#include <dunya/field/field.h>
#include <app/flycontroller/flycontroller.h>
#include <app/demodriver/demodriver.h>
#include <app/framecheck/framecheck.h>
#include <dunya/assets/assetlibrary/assetlibrary.h>
#include <app/debugui/debugui.h>
#include <dunya/core/panels/panels.h>
#include <app/startupoptions/startupoptions.h>
#include <dunya/renderer/sdfrecordtable/sdfrecordtable.h>
#include <dunya/renderer/framepacker/framepacker.h>
#include <dunya/renderer/sdfresidency/sdfresidency.h>
#include <dunya/renderer/volumepool/volumepool.h>
#include <dunya/renderer/sdfbaker/sdfbaker.h>

#include <array>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

class Application {
public:
  Application(const StartupOptions& options, DebugUiFactory tools = {});
  ~Application();

  Application(const Application&) = delete;
  Application& operator=(const Application&) = delete;
  Application(Application&&) = delete;
  Application& operator=(Application&&) = delete;

  int start(const StartupOptions& options = {});

private:
  void lookThrough(float aspect);

  void loadWorld(const StartupOptions& options);

  int exportProject(const StartupOptions& options);

  void frameCamera();

  glm::vec3 groundPoint(float u, float v) const;

  void carveForMeasurement(uint32_t count);

  void handleKeyEvent(const dunya::platform::KeyEvent& event);
  void handleMouseButtonEvent(const dunya::platform::MouseButtonEvent& event);

  void play();

  enum class Stall : uint8_t {
    None,
    Announced,
    Working
  };

  enum class Transition : uint8_t {
    None,
    ToRuntime,
    ToAuthoring,
    Restart
  };

  void announce(std::string text, Transition transition);

  void fire(const glm::vec3& aim);

  glm::vec3 aimAtCursor() const;

  glm::vec3 aimAtTarget() const;

  glm::vec3 aimAtPoint(const glm::vec3& target) const;
  void stop();

  void dent(uint32_t count);

  void recordSceneTelemetry();

  void uploadDentedVolumes();

  dunya::objectmodel::World& activeWorld() noexcept;

  const dunya::objectmodel::World& activeWorld() const noexcept;

  bool acceptsInput() const noexcept;

  dunya::platform::GLFWLibrary m_glfwLibrary;
  dunya::platform::Window m_window;
  GlfwWindowSystem m_windowSystem;

  dunya::gpu::Context m_context;

  dunya::physics::JoltLibrary m_joltLibrary;
  dunya::platform::Input m_input;
  FlyController m_flyController;
  dunya::gpu::SwapChain m_swapChain;
  dunya::objectmodel::World m_authoredWorld;

  dunya::assets::AssetLibrary m_assetLibrary;

  std::optional<dunya::runtime::Runtime> m_runtime;

  dunya::gpu::Uploader m_uploader;

  dunya::renderer::FrameGlobals m_frameGlobals;
  dunya::renderer::ResourceTable m_resourceTable;
  dunya::renderer::SdfRecordTable m_recordTable;
  dunya::renderer::SdfBaker m_sdfBaker;
  dunya::renderer::VolumePool m_volumePool;

  dunya::renderer::SdfResidency m_residency;

  dunya::renderer::FramePacker m_framePacker;

  bool m_splitFailureReported = false;

  dunya::gpu::Pipeline m_meshPipeline;
  dunya::gpu::Pipeline m_sdfPipeline;
  dunya::renderer::Renderer m_renderer;

  std::unique_ptr<DebugUi> m_debugUi;

  dunya::core::Panels m_panels;

  Stall m_stall = Stall::None;
  Transition m_transition = Transition::None;

  std::deque<dunya::objectmodel::Entity> m_balls;

  Projectile m_shotSettings;

  dunya::field::SampledSdf m_projectileField;

  dunya::objectmodel::WorldExtent m_groundExtent;

  uint32_t m_ballVolume = UINT32_MAX;

  JPH::ShapeRefC m_ballShape;

  dunya::renderer::Frame m_frameContext{};
  bool m_reloadRequested;

  double m_lastFrameMs = 0.0;

  dunya::core::Telemetry m_telemetry;

  dunya::runtime::Deformation m_deformation;

  DemoDriver m_demo;

  uint32_t m_frameIndex = 0;

  uint32_t m_dentsApplied = 0;

  uint32_t m_pendingDents = 0;
  std::string m_dentLogPath;

  dunya::core::EventDispatcher::SubscriptionId m_keySubscription{};
  dunya::core::EventDispatcher::SubscriptionId m_mouseSubscription{};
};

constexpr const char* modeName(dunya::gpu::PipelineType type) noexcept {
  switch (type) {
    case dunya::gpu::PipelineType::Mesh:
      return "mesh ";
    case dunya::gpu::PipelineType::Sdf:
      return "field";
    case dunya::gpu::PipelineType::Both:
      return "both ";
    default:
      return "?    ";
  }
}

constexpr dunya::gpu::PipelineType nextPipelineType(
  dunya::gpu::PipelineType current
) noexcept {
  using Value = std::underlying_type_t<dunya::gpu::PipelineType>;

  const auto next = (static_cast<Value>(current) + 1)
                    % static_cast<Value>(dunya::gpu::PipelineType::Count);

  return static_cast<dunya::gpu::PipelineType>(next);
}
