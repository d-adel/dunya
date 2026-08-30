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
#include <scene/scene.h>
#include <dunya/renderer/renderer.h>
#include <dunya/objectmodel/camera/camera.h>
#include <dunya/platform/input/input.h>
#include <dunya/renderer/frame/frame.h>
#include <dunya/renderer/resourcetable/resourcetable.h>
#include <dunya/renderer/frameglobals/frameglobals.h>
#include <dunya/field/field.h>
#include <cameracontroller/cameracontroller.h>
#include <demodriver/demodriver.h>
#include <framecheck/framecheck.h>
#include <tools/tools.h>
#include <dunya/core/panels/panels.h>
#include <startupoptions/startupoptions.h>
#include <dunya/renderer/fieldrecordtable/fieldrecordtable.h>
#include <dunya/renderer/fieldresidency/fieldresidency.h>
#include <dunya/renderer/volumepool/volumepool.h>
#include <dunya/renderer/fieldbaker/fieldbaker.h>
#include <dunya/editor/commandhistory/commandhistory.h>

#include <array>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

class Application {
public:
  Application(const StartupOptions& options, ToolsFactory tools = {});
  ~Application();

  Application(const Application&) = delete;
  Application& operator=(const Application&) = delete;
  Application(Application&&) = delete;
  Application& operator=(Application&&) = delete;

  int start(const StartupOptions& options = {});

private:
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

  dunya::gpu::Context m_context;

  dunya::physics::JoltLibrary m_joltLibrary;
  dunya::platform::Input m_input;
  CameraController m_cameraController;
  dunya::gpu::SwapChain m_swapChain;
  dunya::objectmodel::World m_authoredWorld;

  Scene m_scene;

  std::optional<dunya::runtime::Runtime> m_runtime;

  dunya::gpu::Uploader m_uploader;

  dunya::renderer::FrameGlobals m_frameGlobals;
  dunya::renderer::ResourceTable m_resourceTable;
  dunya::renderer::FieldRecordTable m_recordTable;
  dunya::renderer::FieldBaker m_fieldBaker;
  dunya::renderer::VolumePool m_volumePool;

  dunya::renderer::FieldResidency m_residency;

  bool m_splitFailureReported = false;

  dunya::gpu::Pipeline m_meshPipeline;
  dunya::gpu::Pipeline m_fieldPipeline;
  dunya::renderer::Renderer m_renderer;

  std::unique_ptr<Tools> m_tools;

  dunya::core::Panels m_panels;

  Stall m_stall = Stall::None;
  Transition m_transition = Transition::None;

  std::deque<dunya::objectmodel::Entity> m_balls;

  Scene::Projectile m_shotSettings;

  uint32_t m_ballVolume = UINT32_MAX;

  JPH::ShapeRefC m_ballShape;

  dunya::renderer::Frame m_frameContext{};
  bool m_reloadRequested;

  double m_lastFrameMs = 0.0;

  dunya::core::Telemetry m_telemetry;

  std::vector<dunya::objectmodel::Entity> m_recordEntities;

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
    case dunya::gpu::PipelineType::Field:
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
