#pragma once

#include <dunya/platform/glfwlibrary/glfwlibrary.h>
#include <dunya/gpu/instance/instance.h>
#include <dunya/platform/window/window.h>
#include <dunya/gpu/surface/surface.h>
#include <dunya/gpu/device/device.h>

namespace dunya::gpu {

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

  const dunya::platform::Window& window() const noexcept;
  dunya::platform::Window& window();

  const Instance& instance() const noexcept;
  const Surface& surface() const noexcept;
  const Device& device() const noexcept;
  Device& device();

private:
  dunya::platform::GLFWLibrary m_glfwLib;
  dunya::platform::Window m_window;
  Instance m_instance;
  Surface m_surface;
  Device m_device;
};

}
