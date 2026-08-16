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
#include "imagecompare/imagecompare.h"

#include <span>
#include <string>

/* What the process was asked to do before the first frame.
 *
 * A measurement harness, not a feature. Reproducing a comparison means running
 * the same scene at the same primitive count in each representation, and a hand
 * on the keyboard reproduces neither between runs. Every option here exists to
 * make a measurement repeatable; none of them changes what the renderer is.
 */
struct StartupOptions {
  StartupOptions() = default;
  explicit StartupOptions(std::span<char*> arguments);

  // Carves this many spheres before the first frame, at fixed positions.
  uint32_t carves = 0;

  // Falls back to the exact field. M17 chose the sampled one, so this asks for
  // the reference rather than for a feature.
  bool analytic = false;

  // Compares the compute bake against a CPU bake of the same primitives.
  bool verifyBake = false;

  // Writes the first presented frame here as a PNG and exits. Empty means run
  // normally.
  std::string screenshot;

  // Compares the first presented frame against this reference and exits with a
  // failing status if it has drifted. Empty means run normally.
  std::string golden;
};

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
  void editField(uint32_t operation);
  void stressField(uint32_t count);
  dunya::image::Bitmap readFrame(VkImage image);
  bool compareToGolden(
    const dunya::image::Bitmap& frame,
    const std::string& path
  );
  bool acceptsInput() const noexcept;

  Context m_context;
  Input m_input;
  Camera m_camera;
  SwapChain m_swapChain;
  Scene m_scene;
  FrameGlobals m_frameGlobals;
  ResourceTable m_resourceTable;
  FieldPass m_fieldPass;
  Pipeline m_meshPipeline;
  Pipeline m_fieldPipeline;
  Renderer m_renderer;

  CameraInput m_cameraInput;
  bool m_prevAcceptsInput;
  // Unity scene-view model: the cursor is visible and clickable by default, and
  // only becomes a look control while the right button is held.
  bool m_looking = false;
  Frame m_frameContext{};
  bool m_reloadRequested;

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
