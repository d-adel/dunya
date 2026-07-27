#pragma once

#include "glfwlibrary/glfwlibrary.h"
#include "instance/instance.h"
#include "window/window.h"
#include "surface/surface.h"
#include "device/device.h"
#include "swapchain/swapchain.h"
#include "pipeline/pipeline.h"
#include "renderer/renderer.h"
#include "descriptors/descriptors.h"
#include "texture/texture.h"
#include "depthimage/depthimage.h"
#include "mesh/mesh.h"
#include "ubo/ubo.h"
#include "camera/camera.h"
#include "input/input.h"

const std::vector<char const*> validationLayers = {
  "VK_LAYER_KHRONOS_validation"
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
  void start();

private:
  void handleKeyEvent(const KeyEvent& event);
  bool acceptsInput() const noexcept;

  GLFWLibrary m_glfwLib;
  Window m_window;
  Input m_input;
  Camera m_camera;
  Instance m_instance;
  Surface m_surface;
  Device m_device;
  SwapChain m_swapChain;
  DepthImage m_depthImage;
  Texture m_texture;
  Mesh m_mesh;
  Descriptors m_descriptors;
  Pipeline m_pipeline;
  Renderer m_renderer;

  CameraInput m_cameraInput;
  bool m_prevAcceptsInput;

  EventDispatcher::SubscriptionId m_keySubscription{};
};
