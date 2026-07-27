#pragma once

#include <GLFW/glfw3.h>
#include <vector>

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

class Instance {
public:
  Instance(const std::vector<char const*>& validationLayers);
  Instance(Instance const&) = delete;
  Instance& operator=(Instance const&) = delete;
  ~Instance();

  const VkInstance& handle() const noexcept;

private:
  void setup(const std::vector<char const*>& validationLayers);
  void createVkInstance(const std::vector<char const*>& validationLayers);
  void setupDebugMessenger();

  std::vector<VkLayerProperties> enumerateInstanceLayerProperties();

  VkInstance m_instance = VK_NULL_HANDLE;
  VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;

  static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
    void* userData
  );
};
