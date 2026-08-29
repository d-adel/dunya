#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>

#include <dunya/physics/impact/impact.h>
#include <dunya/physics/joltlibrary/joltlibrary.h>
#include <dunya/physics/physicsworld/physicsworld.h>
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
  explicit Application(const StartupOptions& options);
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

  // Turns the impacts the last step recorded into craters on whatever they
  // struck that is Deformable. This is the milestone: a dent that a collision
  // caused, in the frame the collision happened.
  void applyImpacts();

  // What a demo run measured: how many craters, and the frame times around
  // them. Percentiles rather than a mean, because a deformation that blew the
  // budget once is exactly what a mean would bury.
  void reportDemo() const;

  // Carves one cutter into one entity's lattice, re-shapes its body, wakes
  // whatever was resting on the region, and queues the change for the GPU.
  // The four have to happen together: a lattice the collision shape does not
  // follow is a hole things roll over.
  void carve(
    dunya::objectmodel::Entity entity,
    const dunya::field::Primitive& cutter
  );

  // One copy per changed entity, at the end of the frame. A dent is a few
  // dozen voxels of a 128-cubed volume and copyFrom submits and waits, so a
  // copy per dent would be the milestone rather than a detail of it.
  void uploadDentedVolumes();

  // Whichever world the frame draws and the volume pool serves.
  dunya::objectmodel::World& activeWorld() noexcept;

  // Every pool slot goes back on a world switch: the sampled field is
  // rebuilt for the new world rather than carried across.
  void releaseAllVolumes();
  // A recording shows the numbers and nothing else: sliders in the corner of
  // a demo reel are clutter, and the two counters that do not move are the
  // whole point of the reel.
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

  // Batches the frame's volume copies into one submission that nothing waits
  // on. Before the renderer, so it is destroyed after it and its own drain
  // happens while the device is still alive.
  dunya::gpu::Uploader m_uploader;

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

  // Which entity has an unsent region and how much of its lattice moved,
  // folded into one box per entity as the dents arrive.
  std::vector<std::pair<dunya::objectmodel::Entity, dunya::field::SampleBox>>
    m_pendingUploads;

  // How a contact impulse becomes a crater. Tunable live, because how much
  // damage a hit should do is a judgement about the demo rather than
  // something the physics derives.
  struct Damage {
    // Closing speed in m / s below which a contact leaves no mark at all.
    // A gate on speed rather than impulse, because a stack of heavy boxes
    // standing still pushes impulse past any fixed number.
    float threshold = 3.0f;

    // Metres of depth per kg m / s, then clamped. Linear because a crater
    // that grows without bound eats the object on the first hard shot.
    float depthPerImpulse = 0.0002f;
    float minimumDepth = 0.03f;
    float maximumDepth = 0.25f;

    // A crater is a spherical cap, so the cutter is wider than it is deep.
    // At one it would be a puncture.
    float radiusPerDepth = 2.5f;

    // Craters carved per frame. A ball entering a wall produces sixteen
    // impacts in one physics update, and sixteen at two and a half
    // milliseconds is three frames of work inside one. The rest wait, which
    // costs nothing visible at sixty hertz and is the difference between a
    // steady demo and a stutter.
    uint32_t perFrame = 3u;

    // And no wider than this much of the struck object's shortest side, so
    // the same shot leaves a dent in a wall and a crater in a floor instead
    // of removing a small object outright. Absolute metres cannot do this:
    // 0.25 m is a scratch on the ground and most of a box.
    float widestFraction = 0.25f;
  } m_damage;

  // Drained once per frame from the physics world. A member rather than a
  // local so the frame loop is not allocating a vector sixty times a second
  // for the frames - most of them - that record nothing.
  std::vector<dunya::physics::Impact> m_impacts;

  // An impact that has been accepted but not yet carved.
  //
  // Local space, and that is the whole reason this type exists rather than
  // deferring the Impact itself: a contact point is world space at the moment
  // of contact, and the body it belongs to has moved by the time a deferred
  // crater is carved. In the field's own frame it cannot go stale.
  struct PendingCrater {
    dunya::objectmodel::Entity entity{};
    glm::vec3 point{0.0f};
    glm::vec3 outward{0.0f};
    float impulse = 0.0f;
  };

  std::vector<PendingCrater> m_pendingCraters;

  // Craters a collision caused, as opposed to m_dentsApplied, which is the
  // harness's sequence index and has to stay tied to it.
  uint32_t m_cratersApplied = 0;

  // Frames the demo run has left, and how many have gone. Zero means the
  // window is being driven by a person rather than by the clock.
  uint32_t m_demoFrames = 0;

  // Frames between scripted shots. Sixty hertz is the reference, so a rate of
  // two shots a second is thirty frames.
  uint32_t m_demoInterval = 240;

  // Shots the scripted run has taken, which is the R2 sequence index that
  // spreads them across the wall.
  uint32_t m_shotsFired = 0;
  uint32_t m_frameIndex = 0;

  // Every frame the demo run measured, so the run can report a distribution
  // rather than the last second's mean. A spike is the thing that matters and
  // an average is exactly what hides it - and the index comes with it, because
  // "the worst frame was 50 ms" and "the worst frame was the one that spawned
  // a ball" are different findings.
  struct DemoFrame {
    uint32_t index = 0;
    float ms = 0.0f;
    uint32_t craters = 0;
    bool fired = false;

    // The two halves of a deformation, separately: carving the lattice and
    // re-shaping the body, then sending the changed region to the GPU. They
    // fail differently, so a frame that spiked has to say which one did it.
    float carveMs = 0.0f;
    float uploadMs = 0.0f;
    float physicsMs = 0.0f;
  };

  std::vector<DemoFrame> m_demoFrameMs;

  // This frame's two halves, filled where they happen and read where the
  // frame is recorded, which is earlier in the loop than either.
  float m_frameCarveMs = 0.0f;
  uint32_t m_cratersReported = 0;
  float m_frameUploadMs = 0.0f;
  float m_framePhysicsMs = 0.0f;

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
