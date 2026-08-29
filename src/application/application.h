#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>

#include <dunya/physics/joltlibrary/joltlibrary.h>
#include <dunya/physics/physicsworld/physicsworld.h>
#include <dunya/runtime/runtime/runtime.h>
#include <dunya/gpu/context/context.h>
#include <dunya/gpu/swapchain/swapchain.h>
#include <scene/scene.h>
#include <dunya/renderer/renderer.h>
#include <dunya/objectmodel/camera/camera.h>
#include <dunya/platform/input/input.h>
#include <dunya/renderer/frame/frame.h>
#include <dunya/renderer/resourcetable/resourcetable.h>
#include <dunya/renderer/frameglobals/frameglobals.h>
#include <dunya/field/field.h>
#include <dunya/editor/fieldeditor/fieldeditor.h>
#include <framecheck/framecheck.h>
#include <overlay/overlay.h>
#include <startupoptions/startupoptions.h>
#include <dunya/renderer/fieldrecordtable/fieldrecordtable.h>
#include <dunya/renderer/volumepool/volumepool.h>
#include <dunya/renderer/fieldbaker/fieldbaker.h>
#include <dunya/editor/commandhistory/commandhistory.h>

#include <array>
#include <deque>
#include <optional>
#include <utility>
#include <vector>

// Wiring, and the loop. Owns the subsystems and hands them each other, but does
// none of their work - an edit lives in FieldEditor, a harness run in
// FrameCheck.
class Application {
public:
  Application();
  ~Application();

  Application(const Application&) = delete;
  Application& operator=(const Application&) = delete;
  Application(Application&&) = delete;
  Application& operator=(Application&&) = delete;

  void clearCameraInput() noexcept;

  // Returns the process exit status, because --golden makes this a test as well
  // as a renderer and a failing comparison has to reach the shell.
  int start(const StartupOptions& options = {});

private:
  void handleKeyEvent(const dunya::platform::KeyEvent& event);
  void handleMouseButtonEvent(const dunya::platform::MouseButtonEvent& event);
  void setLookMode(bool looking);

  // Play instantiates a runtime from the authored world and Stop destroys it.
  // Physics lives inside Runtime, so it cannot touch authored state at all.
  void play();

  // A stall the loop is about to take, announced one frame early. The frame
  // that presents the message bakes nothing, and the frame after does the
  // work — so what stays on screen while the loop blocks is the message.
  enum class Stall : uint8_t {
    None,
    Announced,
    Working
  };

  // What the stall is for. None means the work is the frame's own baking.
  enum class Transition : uint8_t {
    None,
    ToRuntime,
    ToAuthoring,
    Restart
  };

  void announce(std::string text, Transition transition);

  // Spawns a ball at the cursor and fires it along the ray. The oldest goes
  // when the cap is reached: each ball holds a volume slot and a body.
  void fire();
  void stop();

  // Whichever world the frame draws and the volume pool serves.
  dunya::objectmodel::World& activeWorld() noexcept;

  // Every pool slot goes back on a world switch: the sampled field is
  // rebuilt for the new world rather than carried across.
  void releaseAllVolumes();
  void registerPanels();
  bool acceptsInput() const noexcept;

  // The cursor as a world ray, which needs the window and the camera and so
  // cannot live with the editing it feeds.
  dunya::field::Ray cursorRay() const;

  dunya::gpu::Context m_context;

  dunya::physics::JoltLibrary m_joltLibrary;
  dunya::platform::Input m_input;
  dunya::objectmodel::Camera m_camera;
  dunya::gpu::SwapChain m_swapChain;
  // The authored world. Play instantiates a runtime beside it; this one is
  // never simulated, so an undo stack can be trusted after a Play session.
  dunya::objectmodel::World m_authoredWorld;

  // Before the runtime, and that is load-bearing: bodies borrow the fields the
  // scene owns, and members are destroyed in reverse declaration order.
  Scene m_scene;

  // Absent while editing. Owns the runtime world and the only PhysicsWorld
  // there is, so E5 holds by construction rather than by a gate.
  std::optional<dunya::runtime::Runtime> m_runtime;

  dunya::renderer::FrameGlobals m_frameGlobals;
  dunya::renderer::ResourceTable m_resourceTable;
  dunya::renderer::FieldRecordTable m_recordTable;
  dunya::renderer::FieldBaker m_fieldBaker;
  dunya::renderer::VolumePool m_volumePool;
  dunya::gpu::Pipeline m_meshPipeline;
  dunya::gpu::Pipeline m_fieldPipeline;
  dunya::renderer::Renderer m_renderer;

  // After the renderer, because it is torn down before the device goes and
  // members are destroyed in reverse declaration order.
  Overlay m_overlay;

  dunya::editor::FieldEditor m_fieldEditor;

  dunya::objectmodel::CameraInput m_cameraInput;
  bool m_prevAcceptsInput;
  // Unity scene-view model: the cursor is visible and clickable by default, and
  // only becomes a look control while the right button is held.
  bool m_looking = false;

  Stall m_stall = Stall::None;
  Transition m_transition = Transition::None;

  // The balls in flight, oldest first. Runtime state, so it empties at Stop.
  std::deque<dunya::objectmodel::Entity> m_balls;

  // The knobs, seeded from the scene and then owned by the panel.
  Scene::Projectile m_shotSettings;

  // One volume for every ball there will ever be. They are the same sphere in
  // their own local space, so a slot each would be the same 16 MiB uploaded
  // over again - measured at 95 ms a shot. Owned by no entity, so the frame
  // loop's sweep never reclaims it.
  uint32_t m_ballVolume = UINT32_MAX;

  // And one shape, for the same reason. Deriving mass and inertia from the
  // grid walks two million cells, and Jolt asks once per body created, so a
  // shape each cost 34 ms a shot - the whole of the spawn.
  JPH::ShapeRefC m_ballShape;

  dunya::renderer::Frame m_frameContext{};
  bool m_reloadRequested;

  // A member rather than a local in start(), because a panel outlives the call
  // that registered it and a captured reference to a local would dangle.
  double m_lastFrameMs = 0.0;

  // Which entity filled each packing slot, recorded as the frame is assembled.
  // Not a position in fields(): a skip consumes no slot, so the two diverge.
  std::vector<dunya::objectmodel::Entity> m_recordEntities;

  // Which entity owns each volume pool slot, so a slot can be reclaimed when
  // its entity goes. Nothing else observes a destroyed field object.
  std::array<dunya::objectmodel::Entity, dunya::core::MAX_FIELD_VOLUMES>
    m_volumeOwners;

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
