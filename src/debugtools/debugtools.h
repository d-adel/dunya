#pragma once

#include <dunya/core/panels/panels.h>
#include <overlay/overlay.h>
#include <tools/tools.h>

class DebugTools final : public Tools {
public:
  DebugTools(
    const dunya::gpu::Context& context,
    const dunya::gpu::SwapChain& swapChain
  );

  DebugTools(const DebugTools&) = delete;
  DebugTools& operator=(const DebugTools&) = delete;
  DebugTools(DebugTools&&) = delete;
  DebugTools& operator=(DebugTools&&) = delete;

  bool wantsMouse() const override;
  bool wantsKeyboard() const override;

  void notice(std::string text) override;

  void build(dunya::core::Panels& registry) override;
  void record(VkCommandBuffer commandBuffer) const override;

  static ToolsFactory factory();

private:
  Overlay m_overlay;
};
