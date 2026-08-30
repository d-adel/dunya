#pragma once

#include <dunya/editor/fieldeditor/fieldeditor.h>
#include <overlay/overlay.h>
#include <application/application.h>
#include <tools/tools.h>

class EditorTools final : public Tools {
public:
  EditorTools(
    const dunya::gpu::Context& context,
    const dunya::gpu::SwapChain& swapChain,
    dunya::objectmodel::World& world
  );

  EditorTools(const EditorTools&) = delete;
  EditorTools& operator=(const EditorTools&) = delete;
  EditorTools(EditorTools&&) = delete;
  EditorTools& operator=(EditorTools&&) = delete;

  bool wantsMouse() const override;
  bool wantsKeyboard() const override;

  void notice(std::string text) override;

  void build(Application& application) override;
  void record(VkCommandBuffer commandBuffer) const override;

  void edit(uint32_t operation, const dunya::field::Ray& ray) override;
  void stress(uint32_t count) override;
  void undo() override;
  void redo() override;
  void retarget(dunya::objectmodel::World& world) override;

  static ToolsFactory factory();

private:
  void registerPanels();

  Overlay m_overlay;
  dunya::editor::FieldEditor m_fieldEditor;

  Application::PanelSources m_sources{};
};
