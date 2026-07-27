#include "swapchain.ih"

SwapChain::SwapChain(
  const VkDevice& device,
  const VkPhysicalDevice& physicalDevice,
  const VkSurfaceKHR& surface,
  GLFWwindow* window
)
    : m_device(device),
      m_physicalDevice(physicalDevice),
      m_surface(surface),
      m_window(window) {
  createSwapChain();
  createImageViews();
}

SwapChain::~SwapChain() {
  cleanup();
}

void SwapChain::cleanup() {
  for (auto imageView : m_swapChainImageViews) {
    vkDestroyImageView(m_device, imageView, nullptr);
  }
  vkDestroySwapchainKHR(m_device, m_swapChain, nullptr);
}

const VkSwapchainKHR& SwapChain::handle() const noexcept {
  return m_swapChain;
}

const VkFormat& SwapChain::imageFormat() const noexcept {
  return m_swapChainImageFormat;
}

const VkExtent2D& SwapChain::swapChainExtent() const noexcept {
  return m_swapChainExtent;
}

const VkImage& SwapChain::image(uint32_t i) const noexcept {
  return m_swapChainImages.at(i);
}

const VkImageView& SwapChain::imageView(uint32_t i) const noexcept {
  return m_swapChainImageViews.at(i);
}

uint32_t SwapChain::imageCount() const noexcept {
  return static_cast<uint32_t>(m_swapChainImageViews.size());
}

void SwapChain::createSwapChain() {
  SwapChainSupportDetails swapChainSupport =
    querySwapChainSupport(m_physicalDevice, m_surface);

  VkSurfaceFormatKHR surfaceFormat =
    chooseSwapSurfaceFormat(swapChainSupport.formats);
  VkPresentModeKHR presentMode =
    chooseSwapPresentMode(swapChainSupport.presentModes);
  VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

  uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
  if (
    swapChainSupport.capabilities.maxImageCount > 0
    && imageCount > swapChainSupport.capabilities.maxImageCount
  ) {
    imageCount = swapChainSupport.capabilities.maxImageCount;
  }

  VkSwapchainCreateInfoKHR createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  createInfo.surface = m_surface;
  createInfo.minImageCount = imageCount;
  createInfo.imageFormat = surfaceFormat.format;
  createInfo.imageColorSpace = surfaceFormat.colorSpace;
  createInfo.imageExtent = extent;
  createInfo.imageArrayLayers = 1;
  createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice, m_surface);
  uint32_t queueFamilyIndices[] = {
    indices.graphicsFamily.value(),
    indices.presentFamily.value()
  };

  if (indices.graphicsFamily != indices.presentFamily) {
    createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    createInfo.queueFamilyIndexCount = 2;
    createInfo.pQueueFamilyIndices = queueFamilyIndices;
  } else {
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  }

  // This is how to pre-transform images
  createInfo.preTransform = swapChainSupport.capabilities.currentTransform;

  // Whether we want to blend the alpha with other windows (no)
  createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

  createInfo.presentMode = presentMode;
  // Clipping = disabled reading back pixels
  createInfo.clipped = VK_TRUE;
  // For resizing
  createInfo.oldSwapchain = m_swapChain;

  if (
    vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapChain)
    != VK_SUCCESS
  ) {
    throw std::runtime_error("Failed to create swap chain");
  }

  if (
    vkGetSwapchainImagesKHR(m_device, m_swapChain, &imageCount, nullptr)
    != VK_SUCCESS
  ) {
    throw std::runtime_error("Failed retrieving image count");
  }

  m_swapChainImages.resize(imageCount);
  if (
    vkGetSwapchainImagesKHR(
      m_device,
      m_swapChain,
      &imageCount,
      m_swapChainImages.data()
    )
    != VK_SUCCESS
  ) {
    throw std::runtime_error("Failed setting the swap chain images");
  }

  m_swapChainExtent = extent;
  m_swapChainImageFormat = surfaceFormat.format;
}

void SwapChain::createImageViews() {
  m_swapChainImageViews.resize(m_swapChainImages.size());

  for (size_t i = 0; i < m_swapChainImages.size(); i++) {
    m_swapChainImageViews[i] = Image::createImageView(
      m_device,
      m_swapChainImages[i],
      m_swapChainImageFormat,
      VK_IMAGE_ASPECT_COLOR_BIT
    );
  }
}

void SwapChain::recreate() {
  int width = 0, height = 0;
  glfwGetFramebufferSize(m_window, &width, &height);
  while (width == 0 || height == 0) {
    glfwGetFramebufferSize(m_window, &width, &height);
    glfwWaitEvents();
  }

  vkDeviceWaitIdle(m_device);

  cleanup();
  m_swapChain = VK_NULL_HANDLE;

  createSwapChain();
  createImageViews();
}

VkSurfaceFormatKHR SwapChain::chooseSwapSurfaceFormat(
  const std::vector<VkSurfaceFormatKHR>& availableFormats
) {
  for (const auto& availableFormat : availableFormats) {
    if (
      availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB
      && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
    ) {
      return availableFormat;
    }
  }

  return availableFormats[0];
}

VkPresentModeKHR SwapChain::chooseSwapPresentMode(
  const std::vector<VkPresentModeKHR>& availablePresentModes
) {
  for (const auto& availablePresentMode : availablePresentModes) {
    if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
      return availablePresentMode;
    }
  }

  // Preferred on mobile devices
  return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D SwapChain::chooseSwapExtent(
  const VkSurfaceCapabilitiesKHR& capabilities
) {
  if (
    capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()
  ) {
    return capabilities.currentExtent;
  } else {
    int width, height;
    glfwGetFramebufferSize(m_window, &width, &height);

    VkExtent2D actualExtent = {
      static_cast<uint32_t>(width),
      static_cast<uint32_t>(height)
    };

    actualExtent.width = std::clamp(
      actualExtent.width,
      capabilities.minImageExtent.width,
      capabilities.maxImageExtent.width
    );
    actualExtent.height = std::clamp(
      actualExtent.height,
      capabilities.minImageExtent.height,
      capabilities.maxImageExtent.height
    );

    return actualExtent;
  }
}
