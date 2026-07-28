#pragma once

#include "context/context.h"
#include "swapchain/swapchain.h"
#include "scene/scene.h"
#include "renderer/renderer.h"
#include "camera/camera.h"
#include "input/input.h"

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
  Renderer m_renderer;

  CameraInput m_cameraInput;
  bool m_prevAcceptsInput;

  EventDispatcher::SubscriptionId m_keySubscription{};
};
