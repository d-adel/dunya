#include "glfwwindowsystem.ih"

GlfwWindowSystem::GlfwWindowSystem(const dunya::platform::Window& window)
    : m_window(window) {}

std::vector<const char*> GlfwWindowSystem::instanceExtensions() const {
  uint32_t count = 0;
  const char** extensions = glfwGetRequiredInstanceExtensions(&count);

  if (extensions == nullptr) {
    throw std::runtime_error("Failed to get required GLFW extensions");
  }

  return std::vector<const char*>(extensions, extensions + count);
}

VkSurfaceKHR GlfwWindowSystem::createSurface(VkInstance instance) const {
  VkSurfaceKHR surface = VK_NULL_HANDLE;

  if (
    glfwCreateWindowSurface(instance, m_window.handle(), nullptr, &surface)
    != VK_SUCCESS
  ) {
    return VK_NULL_HANDLE;
  }

  return surface;
}

VkExtent2D GlfwWindowSystem::framebufferExtent() const {
  int width = 0;
  int height = 0;
  glfwGetFramebufferSize(m_window.handle(), &width, &height);

  return VkExtent2D{
    static_cast<uint32_t>(width),
    static_cast<uint32_t>(height)
  };
}

void GlfwWindowSystem::waitForNonZeroExtent() const {
  VkExtent2D extent = framebufferExtent();

  while (extent.width == 0 || extent.height == 0) {
    glfwWaitEvents();
    extent = framebufferExtent();
  }
}
