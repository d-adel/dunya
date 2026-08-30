#include "overlay.ih"

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
    throw std::runtime_error("Failed to create the overlay descriptor pool");
  }

  return pool;
}

}  // namespace

Overlay::Overlay(
  const dunya::gpu::Context& context,
  const dunya::gpu::SwapChain& swapChain
)
    : m_context(context), m_pool(createPool(context.device().vkDevice())) {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();

  if (!ImGui_ImplGlfw_InitForVulkan(m_context.window().handle(), true)) {
    throw std::runtime_error("Failed to attach the overlay to the window");
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
    throw std::runtime_error("Failed to initialise the overlay renderer");
  }
}

Overlay::~Overlay() {
  m_context.device().waitIdle();

  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  vkDestroyDescriptorPool(m_context.device().vkDevice(), m_pool, nullptr);
}

void Overlay::begin() {
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
}

void Overlay::panel(std::string name, std::function<void()> draw) {
  m_panels.push_back({std::move(name), std::move(draw), true});
}

void Overlay::build() {
  if (ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu("Panels")) {
      for (Panel& panel : m_panels) {
        ImGui::MenuItem(panel.name.c_str(), nullptr, &panel.visible);
      }

      ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
  }

  for (Panel& panel : m_panels) {
    if (!panel.visible) {
      continue;
    }

    if (ImGui::Begin(panel.name.c_str(), &panel.visible)) {
      panel.draw();
    }

    ImGui::End();
  }

  drawNotice();
}

void Overlay::notice(std::string text) {
  m_notice = std::move(text);
}

void Overlay::drawNotice() {
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

void Overlay::end() {
  ImGui::Render();
}

void Overlay::record(VkCommandBuffer commandBuffer) const {
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
}

bool Overlay::wantsMouse() const {
  if constexpr (!enableOverlay) {
    return false;
  } else {
    return ImGui::GetIO().WantCaptureMouse;
  }
}

bool Overlay::wantsKeyboard() const {
  if constexpr (!enableOverlay) {
    return false;
  } else {
    return ImGui::GetIO().WantCaptureKeyboard;
  }
}
