#include "surface.ih"

Surface::Surface(const VkInstance& instance, GLFWwindow* window)
    : m_instance(instance), m_window(window) {
  createSurface();
}

Surface::~Surface() {
  vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
}

const VkSurfaceKHR& Surface::handle() const noexcept {
  return m_surface;
}

void Surface::createSurface() {
  if (
    glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surface)
    != VK_SUCCESS
  ) {
    throw std::runtime_error("failed to create window surface!");
  }
}
