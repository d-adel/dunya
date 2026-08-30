#include "surface.ih"

namespace dunya::gpu {

Surface::Surface(const VkInstance& instance, const WindowSystem& windowSystem)
    : m_instance(instance), m_surface(windowSystem.createSurface(instance)) {
  if (m_surface == VK_NULL_HANDLE) {
    throw std::runtime_error("failed to create window surface!");
  }
}

Surface::~Surface() {
  vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
}

const VkSurfaceKHR& Surface::handle() const noexcept {
  return m_surface;
}

}
