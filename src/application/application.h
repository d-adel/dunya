#pragma once

#include "context/context.h"
#include "swapchain/swapchain.h"
#include "scene/scene.h"
#include "renderer/renderer.h"
#include "camera/camera.h"
#include "input/input.h"
#include "frame/frame.h"

class Application {
public:
  Application();
  ~Application();

  Application(const Application&) = delete;
  Application& operator=(const Application&) = delete;
  Application(Application&&) = delete;
  Application& operator=(Application&&) = delete;

  void clearCameraInput() noexcept;
  void start();

private:
  void handleKeyEvent(const KeyEvent& event);
  bool acceptsInput() const noexcept;

  Context m_context;
  Input m_input;
  Camera m_camera;
  SwapChain m_swapChain;
  Scene m_scene;
  Pipeline m_meshPipeline;
  Pipeline m_fieldPipeline;
  Renderer m_renderer;

  CameraInput m_cameraInput;
  bool m_prevAcceptsInput;
  Frame m_frameContext{};
  bool m_reloadRequested;

  EventDispatcher::SubscriptionId m_keySubscription{};
};

constexpr PipelineType nextPipelineType(PipelineType current) noexcept {
  using Value = std::underlying_type_t<PipelineType>;

  const auto next =
    (static_cast<Value>(current) + 1) % static_cast<Value>(PipelineType::Count);

  return static_cast<PipelineType>(next);
}
