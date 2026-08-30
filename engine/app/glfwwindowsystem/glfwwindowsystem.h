#pragma once

#include <vulkan/vulkan.h>

#include <dunya/gpu/windowsystem/windowsystem.h>
#include <dunya/platform/window/window.h>

#include <vector>

class GlfwWindowSystem final : public dunya::gpu::WindowSystem {
public:
  explicit GlfwWindowSystem(const dunya::platform::Window& window);

  GlfwWindowSystem(const GlfwWindowSystem&) = delete;
  GlfwWindowSystem& operator=(const GlfwWindowSystem&) = delete;
  GlfwWindowSystem(GlfwWindowSystem&&) = delete;
  GlfwWindowSystem& operator=(GlfwWindowSystem&&) = delete;

  std::vector<const char*> instanceExtensions() const override;
  VkSurfaceKHR createSurface(VkInstance instance) const override;
  VkExtent2D framebufferExtent() const override;
  void waitForNonZeroExtent() const override;

private:
  const dunya::platform::Window& m_window;
};
