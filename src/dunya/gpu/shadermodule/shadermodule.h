#pragma once

#include <vulkan/vulkan.h>

#include <string>
#include <utility>
#include <vector>

namespace dunya::gpu {

class ShaderModule {
public:
  ShaderModule(const VkDevice& device, const std::vector<char>& code);

  // Reading the SPIR-V is the module's business, not its caller's. Two
  // pipelines needed it and a second copy of the loader was the alternative.
  ShaderModule(const VkDevice& device, const std::string& path);

  ShaderModule(const ShaderModule&) = delete;
  ShaderModule& operator=(const ShaderModule&) = delete;

  ShaderModule(ShaderModule&& other) noexcept;
  ShaderModule& operator=(ShaderModule&& other) noexcept;

  ~ShaderModule();

  VkShaderModule handle() const noexcept;

private:
  void destroy() noexcept;

  VkShaderModule m_module = VK_NULL_HANDLE;
  VkDevice m_device = VK_NULL_HANDLE;
};

}  // namespace dunya::gpu
