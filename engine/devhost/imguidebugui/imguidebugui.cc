#include "imguidebugui.ih"

namespace {

constexpr uint32_t POOL_CAPACITY = 8;

VkDescriptorPool createPool(VkDevice device) {
  VkDescriptorPoolSize size{};
  size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  size.descriptorCount = POOL_CAPACITY;

  VkDescriptorPoolCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  info.maxSets = POOL_CAPACITY;
  info.poolSizeCount = 1;
  info.pPoolSizes = &size;

  VkDescriptorPool pool = VK_NULL_HANDLE;

  if (vkCreateDescriptorPool(device, &info, nullptr, &pool) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create the debug UI descriptor pool");
  }

  return pool;
}

}

ImGuiDebugUi::ImGuiDebugUi(
  const dunya::platform::Window& window,
  const dunya::gpu::Context& context,
  const dunya::gpu::SwapChain& swapChain
)
    : m_context(context), m_pool(createPool(context.device().vkDevice())) {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();

  if (!ImGui_ImplGlfw_InitForVulkan(window.handle(), true)) {
    throw std::runtime_error("Failed to attach the debug UI to the window");
  }

  const VkFormat colorFormat = swapChain.imageFormat();

  VkPipelineRenderingCreateInfo rendering{};
  rendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
  rendering.colorAttachmentCount = 1;
  rendering.pColorAttachmentFormats = &colorFormat;

  rendering.depthAttachmentFormat = swapChain.depthImage().format();

  ImGui_ImplVulkan_InitInfo info{};
  info.Instance = m_context.instance().handle();
  info.PhysicalDevice = m_context.device().physicalDevice();
  info.Device = m_context.device().vkDevice();
  info.QueueFamily = m_context.device().graphicsFamilyIndex();
  info.Queue = m_context.device().graphicsQueue();
  info.DescriptorPool = m_pool;
  info.MinImageCount = swapChain.imageCount();
  info.ImageCount = swapChain.imageCount();
  info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

  info.UseDynamicRendering = true;
  info.PipelineRenderingCreateInfo = rendering;

  if (!ImGui_ImplVulkan_Init(&info)) {
    throw std::runtime_error("Failed to initialise the debug UI renderer");
  }
}

ImGuiDebugUi::~ImGuiDebugUi() {
  m_context.device().waitIdle();

  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  vkDestroyDescriptorPool(m_context.device().vkDevice(), m_pool, nullptr);
}

void ImGuiDebugUi::begin() {
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
}

namespace {

void drawWidget(const dunya::debugui::Widget& widget) {
  switch (widget.kind) {
    case dunya::debugui::WidgetKind::Separator:
      ImGui::Separator();
      return;

    case dunya::debugui::WidgetKind::Button:
      if (ImGui::Button(widget.name.c_str()) && widget.press) {
        widget.press();
      }
      return;

    case dunya::debugui::WidgetKind::Text:
      if (widget.text) {
        ImGui::Text("%s  %s", widget.name.c_str(), widget.text().c_str());
      }
      return;

    case dunya::debugui::WidgetKind::Toggle: {
      if (!widget.read) {
        return;
      }

      bool on = widget.read() != 0.0;

      if (ImGui::Checkbox(widget.name.c_str(), &on) && widget.write) {
        widget.write(on ? 1.0 : 0.0);
      }

      return;
    }

    case dunya::debugui::WidgetKind::Slider: {
      if (!widget.read) {
        return;
      }

      float held = static_cast<float>(widget.read());

      const std::string format =
        widget.unit.empty() ? std::string("%.4f") : "%.4f " + widget.unit;

      if (
        ImGui::SliderFloat(
          widget.name.c_str(),
          &held,
          static_cast<float>(widget.minimum),
          static_cast<float>(widget.maximum),
          format.c_str()
        )
        && widget.write
      ) {
        widget.write(static_cast<double>(held));
      }

      return;
    }

    case dunya::debugui::WidgetKind::Value:
      if (widget.read) {
        ImGui::Text(
          "%-18s %10.3f %s",
          widget.name.c_str(),
          widget.read(),
          widget.unit.c_str()
        );
      }

      return;
  }
}

}

void ImGuiDebugUi::build(dunya::debugui::Panels& registry) {
  begin();

  if (ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu("Panels")) {
      for (dunya::debugui::Panel& panel : registry.panels()) {
        bool on = panel.visible();

        if (ImGui::MenuItem(panel.name().c_str(), nullptr, &on)) {
          panel.show(on);
        }
      }

      ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
  }

  for (dunya::debugui::Panel& panel : registry.panels()) {
    if (!panel.visible()) {
      continue;
    }

    bool open = true;

    if (ImGui::Begin(panel.name().c_str(), &open)) {
      for (const dunya::debugui::Widget& widget : panel.widgets()) {
        drawWidget(widget);
      }
    }

    ImGui::End();

    panel.show(open);
  }

  drawNotice();

  end();
}

void ImGuiDebugUi::notice(std::string text) {
  m_notice = std::move(text);
}

void ImGuiDebugUi::drawNotice() {
  if (m_notice.empty()) {
    return;
  }

  const ImVec2 size(340.0f, 58.0f);
  const ImVec2 centre = ImGui::GetMainViewport()->GetCenter();

  ImGui::SetNextWindowPos(
    ImVec2(centre.x - size.x * 0.5f, centre.y - size.y * 0.5f)
  );
  ImGui::SetNextWindowSize(size);

  const ImGuiWindowFlags flags =
    ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove
    | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav
    | ImGuiWindowFlags_NoInputs;

  if (ImGui::Begin("##notice", nullptr, flags)) {
    const float width = ImGui::CalcTextSize(m_notice.c_str()).x;

    ImGui::SetCursorPos(ImVec2(
      (size.x - width) * 0.5f,
      (size.y - ImGui::GetTextLineHeight()) * 0.5f
    ));
    ImGui::TextUnformatted(m_notice.c_str());
  }

  ImGui::End();
}

void ImGuiDebugUi::end() {
  ImGui::Render();
}

void ImGuiDebugUi::record(VkCommandBuffer commandBuffer) const {
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
}

bool ImGuiDebugUi::wantsMouse() const {
  return ImGui::GetIO().WantCaptureMouse;
}

bool ImGuiDebugUi::wantsKeyboard() const {
  return ImGui::GetIO().WantCaptureKeyboard;
}

DebugUiFactory ImGuiDebugUi::factory() {
  return [](
           const dunya::platform::Window& window,
           const dunya::gpu::Context& context,
           const dunya::gpu::SwapChain& swapChain
         ) -> std::unique_ptr<DebugUi> {
    return std::make_unique<ImGuiDebugUi>(window, context, swapChain);
  };
}
