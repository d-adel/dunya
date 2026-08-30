#pragma once

#include <dunya/core/panels/panels.h>
#include <dunya/gpu/context/context.h>
#include <dunya/gpu/swapchain/swapchain.h>

#include <vulkan/vulkan.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#ifdef DUNYA_EDITOR
inline constexpr bool enableOverlay = true;
#else
inline constexpr bool enableOverlay = false;
#endif

class Overlay {
public:
  Overlay(
    const dunya::gpu::Context& context,
    const dunya::gpu::SwapChain& swapChain
  );

  Overlay(const Overlay&) = delete;
  Overlay& operator=(const Overlay&) = delete;
  Overlay(Overlay&&) = delete;
  Overlay& operator=(Overlay&&) = delete;

  ~Overlay();

  void notice(std::string text);

  void begin();
  void build(dunya::core::Panels& registry);
  void end();

  void record(VkCommandBuffer commandBuffer) const;

  bool wantsMouse() const;
  bool wantsKeyboard() const;

private:
  void drawNotice();

  const dunya::gpu::Context& m_context;

  std::string m_notice;

  VkDescriptorPool m_pool = VK_NULL_HANDLE;
};
