#include "editortools.ih"

EditorTools::EditorTools(
  const dunya::gpu::Context& context,
  const dunya::gpu::SwapChain& swapChain,
  dunya::objectmodel::World& world
)
    : m_overlay(context, swapChain), m_fieldEditor(world) {
  registerPanels();
}

ToolsFactory EditorTools::factory() {
  return [](
           const dunya::gpu::Context& context,
           const dunya::gpu::SwapChain& swapChain,
           dunya::objectmodel::World& world
         ) -> std::unique_ptr<Tools> {
    return std::make_unique<EditorTools>(context, swapChain, world);
  };
}

bool EditorTools::wantsMouse() const {
  return m_overlay.wantsMouse();
}

bool EditorTools::wantsKeyboard() const {
  return m_overlay.wantsKeyboard();
}

void EditorTools::notice(std::string text) {
  m_overlay.notice(std::move(text));
}

void EditorTools::build(Application& application) {
  m_sources = application.panelSources();

  m_overlay.begin();
  m_overlay.build();
  m_overlay.end();
}

void EditorTools::record(VkCommandBuffer commandBuffer) const {
  m_overlay.record(commandBuffer);
}

void EditorTools::edit(uint32_t operation, const dunya::field::Ray& ray) {
  m_fieldEditor.edit(operation, ray);
}

void EditorTools::stress(uint32_t count) {
  m_fieldEditor.stress(count);
}

void EditorTools::undo() {
  m_fieldEditor.undo();
}

void EditorTools::redo() {
  m_fieldEditor.redo();
}

void EditorTools::retarget(dunya::objectmodel::World& world) {
  m_fieldEditor.retarget(world);
}

void EditorTools::registerPanels() {
  m_overlay.panel("Dunya", [this] {
    if (m_sources.world == nullptr || m_sources.deformation == nullptr) {
      return;
    }

    panels::dunya(
      *m_sources.world,
      *m_sources.deformation,
      m_sources.playing,
      m_sources.frameMs
    );
  });

  m_overlay.panel("Damage", [this] {
    if (m_sources.deformation == nullptr) {
      return;
    }

    panels::damage(*m_sources.deformation, m_sources.impacts);
  });

  m_overlay.panel("Shot", [this] {
    if (m_sources.shot == nullptr) {
      return;
    }

    panels::shot(
      *m_sources.shot,
      m_sources.balls,
      m_sources.maxBalls,
      {m_sources.fire, m_sources.resetWall}
    );
  });

  m_overlay.panel("Frame", [this] {
    panels::frame(
      m_sources.frameMs,
      m_sources.extent,
      m_sources.primitives,
      m_sources.analytic
    );
  });

  m_overlay.panel("March", [this] {
    if (m_sources.march == nullptr) {
      return;
    }

    panels::march(*m_sources.march);
  });
}
