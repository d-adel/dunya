#pragma once

#include "platform/glfwlibrary/glfwlibrary.h"
#include "gpu/instance/instance.h"
#include "platform/window/window.h"
#include "gpu/surface/surface.h"
#include "gpu/device/device.h"

const std::vector<char const*> validationLayers = {
  "VK_LAYER_KHRONOS_validation"
};

class Context {
public:
  Context();
  ~Context() = default;

  Context(const Context&) = delete;
  Context& operator=(const Context&) = delete;
  Context(Context&&) = delete;
  Context& operator=(Context&&) = delete;

  const Window& window() const noexcept;
  Window& window();

  const Instance& instance() const noexcept;
  const Surface& surface() const noexcept;
  const Device& device() const noexcept;
  Device& device();

private:
  GLFWLibrary m_glfwLib;
  Window m_window;
  Instance m_instance;
  Surface m_surface;
  Device m_device;
};
