#pragma once

#include <GLFW/glfw3.h>
#include "device/device.h"
#include <vector>

class SwapChain {
public:
  SwapChain(
    const VkDevice& device,
    const VkPhysicalDevice& physicalDeivce,
    const VkSurfaceKHR& surface,
    GLFWwindow* window
  );
  SwapChain(SwapChain const&) = delete;
  SwapChain& operator=(SwapChain const&) = delete;
  ~SwapChain();

  void recreate();
  const VkSwapchainKHR& handle() const noexcept;
  const VkFormat& imageFormat() const noexcept;
  const VkExtent2D& swapChainExtent() const noexcept;
  const VkImage& image(uint32_t i) const noexcept;
  const VkImageView& imageView(uint32_t i) const noexcept;
  uint32_t imageCount() const noexcept;

private:
  void createSwapChain();
  void createImageViews();
  void cleanup();
  VkSurfaceFormatKHR chooseSwapSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR>& availableFormats
  );
  VkPresentModeKHR chooseSwapPresentMode(
    const std::vector<VkPresentModeKHR>& availablePresentModes
  );
  VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

  VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
  VkDevice m_device = VK_NULL_HANDLE;
  VkSurfaceKHR m_surface = VK_NULL_HANDLE;

  VkSwapchainKHR m_swapChain = VK_NULL_HANDLE;
  VkFormat m_swapChainImageFormat = VK_FORMAT_UNDEFINED;
  VkExtent2D m_swapChainExtent;
  std::vector<VkImage> m_swapChainImages;
  std::vector<VkImageView> m_swapChainImageViews;

  GLFWwindow* m_window;
};
