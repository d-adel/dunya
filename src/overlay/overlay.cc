#include "overlay.ih"

namespace {

// One kind, because that is all the backend allocates: a combined sampler per
// texture it is asked to draw, which today is the font atlas and nothing else.
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

Overlay::Overlay(const Context& context, const SwapChain& swapChain)
    : m_context(context), m_pool(createPool(context.device().vkDevice())) {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();

  // true installs ImGui's GLFW callbacks, which chain to the ones already
  // registered rather than replacing them, so this project's own input keeps
  // receiving everything it did before.
  if (!ImGui_ImplGlfw_InitForVulkan(m_context.window().handle(), true)) {
    throw std::runtime_error("Failed to attach the overlay to the window");
  }

  const VkFormat colorFormat = swapChain.imageFormat();

  VkPipelineRenderingCreateInfo rendering{};
  rendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
  rendering.colorAttachmentCount = 1;
  rendering.pColorAttachmentFormats = &colorFormat;

  // The overlay writes no depth, but it is recorded inside a pass that has a
  // depth attachment bound, and a pipeline's declared formats must match the
  // pass it is used in whether or not it touches them.
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

  // There is no VkRenderPass to hand it: this project has used dynamic
  // rendering since M4, so the backend is told the attachment format instead.
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
  // The menu is the only chrome the overlay owns itself, because hiding a panel
  // is the overlay's business and nothing a panel should have to implement.
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

    // Begin returns false when the window is collapsed, and End is still
    // required either way - which is why the draw is skipped rather than the
    // pair.
    if (ImGui::Begin(panel.name.c_str(), &panel.visible)) {
      panel.draw();
    }

    ImGui::End();
  }
}

void Overlay::end() {
  ImGui::Render();
}

// A window ImGui has just met produces no geometry on its first frame: it is
// hidden for one frame while its auto-fit size is measured. That is normal, and
// it is why a single-frame capture is a poor way to check the overlay works.
void Overlay::record(VkCommandBuffer commandBuffer) const {
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
}

bool Overlay::wantsMouse() const {
  return ImGui::GetIO().WantCaptureMouse;
}

bool Overlay::wantsKeyboard() const {
  return ImGui::GetIO().WantCaptureKeyboard;
}
