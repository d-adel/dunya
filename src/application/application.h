#pragma once

#include <dunya/physics/joltlibrary/joltlibrary.h>
#include <dunya/physics/physicsworld/physicsworld.h>
#include <physicsdemo/physicsdemo.h>
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

/* Wiring, and the loop.
 *
 * Owns the subsystems and hands them each other; turns input into intent and
 * intent into calls on whichever of them the intent belongs to. What it should
 * *not* do is any of their work - an edit lives in FieldEditor, a harness run
 * in FrameCheck, argument parsing in StartupOptions.
 */
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
  void registerPanels();
  bool acceptsInput() const noexcept;

  // The cursor as a world ray, which needs the window and the camera and so
  // cannot live with the editing it feeds.
  dunya::field::Ray cursorRay() const;

  dunya::gpu::Context m_context;

  dunya::physics::JoltLibrary m_joltLibrary;
  dunya::physics::PhysicsWorld m_physicsWorld;
  // Important: demo after library and world initialization
  PhysicsDemo m_physicsDemo;

  dunya::platform::Input m_input;
  dunya::objectmodel::Camera m_camera;
  dunya::gpu::SwapChain m_swapChain;
  Scene m_scene;
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
  dunya::renderer::Frame m_frameContext{};
  bool m_reloadRequested;

  // A member rather than a local in start(), because a panel outlives the call
  // that registered it and a captured reference to a local would dangle.
  double m_lastFrameMs = 0.0;

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
