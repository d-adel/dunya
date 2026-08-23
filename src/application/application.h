#pragma once

#include "context/context.h"
#include "swapchain/swapchain.h"
#include "scene/scene.h"
#include "renderer/renderer.h"
#include "camera/camera.h"
#include "input/input.h"
#include "frame/frame.h"
#include "resourcetable/resourcetable.h"
#include "fieldpass/fieldpass.h"
#include "frameglobals/frameglobals.h"
#include "field/field.h"
#include "fieldeditor/fieldeditor.h"
#include "framecheck/framecheck.h"
#include "overlay/overlay.h"
#include "startupoptions/startupoptions.h"
#include "fieldobjecttable/fieldobjecttable.h"
#include "volumepool/volumepool.h"

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
  void handleKeyEvent(const KeyEvent& event);
  void handleMouseButtonEvent(const MouseButtonEvent& event);
  void setLookMode(bool looking);
  void registerPanels();
  bool acceptsInput() const noexcept;

  // The cursor as a world ray, which needs the window and the camera and so
  // cannot live with the editing it feeds.
  dunya::field::Ray cursorRay() const;

  Context m_context;
  Input m_input;
  Camera m_camera;
  SwapChain m_swapChain;
  Scene m_scene;
  FrameGlobals m_frameGlobals;
  ResourceTable m_resourceTable;
  FieldObjectTable m_fieldObjectTable;
  VolumePool m_volumePool;
  FieldPass m_fieldPass;
  Pipeline m_meshPipeline;
  Pipeline m_fieldPipeline;
  Renderer m_renderer;

  // After the renderer, because it is torn down before the device goes and
  // members are destroyed in reverse declaration order.
  Overlay m_overlay;

  FieldEditor m_fieldEditor;

  CameraInput m_cameraInput;
  bool m_prevAcceptsInput;
  // Unity scene-view model: the cursor is visible and clickable by default, and
  // only becomes a look control while the right button is held.
  bool m_looking = false;
  Frame m_frameContext{};
  bool m_reloadRequested;

  // A member rather than a local in start(), because a panel outlives the call
  // that registered it and a captured reference to a local would dangle.
  double m_lastFrameMs = 0.0;

  EventDispatcher::SubscriptionId m_keySubscription{};
  EventDispatcher::SubscriptionId m_mouseSubscription{};
};

constexpr const char* modeName(PipelineType type) noexcept {
  switch (type) {
    case PipelineType::Mesh:
      return "mesh ";
    case PipelineType::Field:
      return "field";
    case PipelineType::Both:
      return "both ";
    default:
      return "?    ";
  }
}

constexpr PipelineType nextPipelineType(PipelineType current) noexcept {
  using Value = std::underlying_type_t<PipelineType>;

  const auto next =
    (static_cast<Value>(current) + 1) % static_cast<Value>(PipelineType::Count);

  return static_cast<PipelineType>(next);
}
