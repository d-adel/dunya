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
#include <dunya/editor/fieldeditor/fieldeditor.h>
#include <cameracontroller/cameracontroller.h>
#include <demodriver/demodriver.h>
#include <framecheck/framecheck.h>
#include <overlay/overlay.h>
#include <startupoptions/startupoptions.h>
#include <dunya/renderer/fieldrecordtable/fieldrecordtable.h>
#include <dunya/renderer/fieldresidency/fieldresidency.h>
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
  explicit Application(const StartupOptions& options);
  ~Application();

  Application(const Application&) = delete;
  Application& operator=(const Application&) = delete;
  Application(Application&&) = delete;
  Application& operator=(Application&&) = delete;

  // Returns the process exit status, because --golden makes this a test as well
  // as a renderer and a failing comparison has to reach the shell.
  int start(const StartupOptions& options = {});

private:
  void handleKeyEvent(const dunya::platform::KeyEvent& event);
  void handleMouseButtonEvent(const dunya::platform::MouseButtonEvent& event);

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

  // Spawns a ball ahead of the camera and throws it along `aim`, which must be
  // a unit direction. The oldest goes when the cap is reached: each ball holds
  // a volume slot and a body.
  void fire(const glm::vec3& aim);

  // Where the cursor is pointing. What a person shooting at a wall means by
  // "there", and the reason fire() takes a direction instead of reading the
  // scene's fixed target: a scripted run has no cursor.
  glm::vec3 aimAtCursor() const;

  // The scene's authored target, for a run with nobody at the keyboard.
  glm::vec3 aimAtTarget() const;

  // A unit direction from the muzzle to a world point.
  glm::vec3 aimAtPoint(const glm::vec3& target) const;
  void stop();

  // Applies dents to the scene's deformable and remembers the region to send
  // to the GPU. Positions come from the R3 sequence the stress carve uses, so
  // they spread evenly through the object and land the same way every run.
  void dent(uint32_t count);

  // What the scene holds, for the scripted run's report. Gathered here because
  // the world, the pool and the runtime are all the loop's.
  [[nodiscard]] DemoDriver::SceneSummary sceneSummary() const;

  // One copy per changed entity, at the end of the frame. A dent is a few
  // dozen voxels of a 128-cubed volume and copyFrom submits and waits, so a
  // copy per dent would be the milestone rather than a detail of it.
  void uploadDentedVolumes();

  // Whichever world the frame draws and the volume pool serves.
  dunya::objectmodel::World& activeWorld() noexcept;

  const dunya::objectmodel::World& activeWorld() const noexcept;

  // A recording shows the numbers and nothing else: sliders in the corner of
  // a demo reel are clutter, and the two counters that do not move are the
  // whole point of the reel.
  void registerPanels();
  bool acceptsInput() const noexcept;

  dunya::gpu::Context m_context;

  dunya::physics::JoltLibrary m_joltLibrary;
  dunya::platform::Input m_input;
  // Flying the camera and the look mode that gates it. After the input, which
  // it holds a reference to.
  CameraController m_cameraController;
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

  // Batches the frame's volume copies into one submission that nothing waits
  // on. Before the renderer, so it is destroyed after it and its own drain
  // happens while the device is still alive.
  dunya::gpu::Uploader m_uploader;

  dunya::renderer::FrameGlobals m_frameGlobals;
  dunya::renderer::ResourceTable m_resourceTable;
  dunya::renderer::FieldRecordTable m_recordTable;
  dunya::renderer::FieldBaker m_fieldBaker;
  dunya::renderer::VolumePool m_volumePool;

  // After the pool, the record table and the uploader, all of which it holds a
  // reference to. Which entity holds which volume slot, and the rules that
  // keep the two agreeing.
  dunya::renderer::FieldResidency m_residency;

  // A pool that is full stays full, so the dropped dent is reported once for
  // the run rather than once a frame.
  bool m_splitFailureReported = false;

  dunya::gpu::Pipeline m_meshPipeline;
  dunya::gpu::Pipeline m_fieldPipeline;
  dunya::renderer::Renderer m_renderer;

  // After the renderer, because it is torn down before the device goes and
  // members are destroyed in reverse declaration order.
  Overlay m_overlay;

  dunya::editor::FieldEditor m_fieldEditor;

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

  // Contacts become craters, the shape follows the geometry, and the changed
  // regions come back out on a queue. It holds no Vulkan, so the copies stay
  // this side of the seam.
  dunya::runtime::Deformation m_deformation;

  // The scripted run: its schedule, its measurements and its report. Zero
  // frames means nobody scripted this one and it answers every question in the
  // negative.
  DemoDriver m_demo;

  uint32_t m_frameIndex = 0;

  // This frame's two halves, filled where they happen and read where the
  // frame is recorded, which is earlier in the loop than either.
  float m_frameCarveMs = 0.0f;
  float m_frameUploadMs = 0.0f;
  float m_framePhysicsMs = 0.0f;

  // What the last frame simulated: how many bodies were awake and how many
  // fixed steps it bought.
  uint32_t m_frameActiveBodies = 0;
  uint32_t m_frameSubsteps = 0;

  // How many dents have been applied, which is the R3 sequence's index and so
  // the thing that makes a run repeatable.
  uint32_t m_dentsApplied = 0;

  // Asked for on the command line and applied by the first frame that has a
  // lattice to apply them to.
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
