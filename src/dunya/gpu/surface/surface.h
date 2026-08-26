#pragma once

#include <GLFW/glfw3.h>

namespace dunya::gpu {

class Surface {
public:
  Surface(const VkInstance& instance, GLFWwindow* window);
  Surface(Surface const&) = delete;
  Surface& operator=(Surface const&) = delete;
  ~Surface();

  const VkSurfaceKHR& handle() const noexcept;

private:
  void createSurface();

  VkInstance m_instance = VK_NULL_HANDLE;
  GLFWwindow* m_window;
  VkSurfaceKHR m_surface = VK_NULL_HANDLE;
};

}  // namespace dunya::gpu
