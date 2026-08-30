#pragma once

#include <dunya/gpu/context/context.h>
#include <dunya/gpu/swapchain/swapchain.h>
#include <dunya/core/panels/panels.h>

#include <vulkan/vulkan.h>

#include <functional>
#include <memory>
#include <string>

class Tools {
public:
  virtual ~Tools() = default;

  virtual bool wantsMouse() const = 0;
  virtual bool wantsKeyboard() const = 0;

  virtual void notice(std::string text) = 0;

  virtual void build(dunya::core::Panels& registry) = 0;
  virtual void record(VkCommandBuffer commandBuffer) const = 0;
};

using ToolsFactory = std::function<std::unique_ptr<
  Tools>(const dunya::gpu::Context&, const dunya::gpu::SwapChain&)>;
