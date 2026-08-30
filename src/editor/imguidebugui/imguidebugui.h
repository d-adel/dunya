#pragma once

#include <app/debugui/debugui.h>
#include <dunya/core/panels/panels.h>
#include <dunya/gpu/context/context.h>
#include <dunya/gpu/swapchain/swapchain.h>

#include <vulkan/vulkan.h>

#include <string>

class ImGuiDebugUi final : public DebugUi {
public:
  ImGuiDebugUi(
    const dunya::gpu::Context& context,
    const dunya::gpu::SwapChain& swapChain
  );

  ImGuiDebugUi(const ImGuiDebugUi&) = delete;
  ImGuiDebugUi& operator=(const ImGuiDebugUi&) = delete;
  ImGuiDebugUi(ImGuiDebugUi&&) = delete;
  ImGuiDebugUi& operator=(ImGuiDebugUi&&) = delete;

  ~ImGuiDebugUi() override;

  bool wantsMouse() const override;
  bool wantsKeyboard() const override;

  void notice(std::string text) override;

  void build(dunya::core::Panels& registry) override;
  void record(VkCommandBuffer commandBuffer) const override;

  static DebugUiFactory factory();

private:
  void begin();
  void end();
  void drawNotice();

  const dunya::gpu::Context& m_context;

  std::string m_notice;

  VkDescriptorPool m_pool = VK_NULL_HANDLE;
};
