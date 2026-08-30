#include "debugtools.ih"

DebugTools::DebugTools(
  const dunya::gpu::Context& context,
  const dunya::gpu::SwapChain& swapChain
)
    : m_overlay(context, swapChain) {}

ToolsFactory DebugTools::factory() {
  return [](
           const dunya::gpu::Context& context,
           const dunya::gpu::SwapChain& swapChain
         ) -> std::unique_ptr<Tools> {
    return std::make_unique<DebugTools>(context, swapChain);
  };
}

bool DebugTools::wantsMouse() const {
  return m_overlay.wantsMouse();
}

bool DebugTools::wantsKeyboard() const {
  return m_overlay.wantsKeyboard();
}

void DebugTools::notice(std::string text) {
  m_overlay.notice(std::move(text));
}

void DebugTools::build(dunya::core::Panels& registry) {
  m_overlay.begin();
  m_overlay.build(registry);
  m_overlay.end();
}

void DebugTools::record(VkCommandBuffer commandBuffer) const {
  m_overlay.record(commandBuffer);
}
