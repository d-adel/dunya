#include "session.ih"

namespace dunya::capi {

namespace {

dunya::gpu::WindowSystem& require(
  const std::unique_ptr<dunya::gpu::WindowSystem>& windowSystem
) {
  if (windowSystem == nullptr) {
    throw std::runtime_error("A session needs a window system");
  }

  return *windowSystem;
}

}

Session::Session(
  std::unique_ptr<dunya::gpu::WindowSystem> windowSystem,
  const std::filesystem::path& projectRoot,
  const std::string& world
)
    : m_windowSystem(std::move(windowSystem)),
      m_context(require(m_windowSystem)),
      m_swapChain(m_context),
      m_assetLibrary(m_context, projectRoot),
      m_uploader(m_context.device()),
      m_frameGlobals(m_context.device()),
      m_resourceTable(
        m_context.device(),
        m_assetLibrary.textures(),
        m_assetLibrary.samplers(),
        m_assetLibrary.materials()
      ),
      m_recordTable(m_context.device()),
      m_sdfBaker(m_context.device(), m_recordTable),
      m_volumePool(m_context.device()),
      m_residency(m_volumePool, m_recordTable, m_uploader),
      m_framePacker(m_volumePool, m_residency, m_recordTable),
      m_meshPipeline(
        dunya::gpu::PipelineType::Mesh,
        m_context.device().vkDevice(),
        std::vector<VkDescriptorSetLayout>{
          m_frameGlobals.setLayout(),
          m_resourceTable.setLayout()
        },
        m_swapChain,
        std::vector<VkVertexInputBindingDescription>{
          dunya::renderer::Vertex::getBindingDescription()
        },
        dunya::renderer::Vertex::getAttributeDescriptions()
      ),
      m_sdfPipeline(
        dunya::gpu::PipelineType::Sdf,
        m_context.device().vkDevice(),
        std::vector<VkDescriptorSetLayout>{
          m_frameGlobals.setLayout(),
          m_resourceTable.setLayout(),
          m_recordTable.setLayout()
        },
        m_swapChain
      ),
      m_renderer(
        m_context.device(),
        m_recordTable,
        m_sdfBaker,
        m_volumePool,
        m_frameGlobals,
        m_meshPipeline,
        m_sdfPipeline,
        m_resourceTable,
        m_context.surface().handle(),
        m_swapChain.imageCount()
      ) {
  loadWorld(projectRoot, world);
}

Session::~Session() {
  m_context.device().waitIdle();
}

void Session::loadWorld(
  const std::filesystem::path& projectRoot,
  const std::string& world
) {
  std::optional<dunya::serialize::Project> project =
    dunya::serialize::Project::open(projectRoot);

  if (!project.has_value()) {
    throw std::runtime_error("No project at " + projectRoot.string());
  }

  dunya::serialize::StoredWorld stored;

  if (!project->loadWorld(world, stored)) {
    throw std::runtime_error("No world named " + world);
  }

  if (!dunya::serialize::restoreWorld(
        stored,
        m_world,
        m_assetLibrary.assets()
      )) {
    throw std::runtime_error("The world " + world + " did not load");
  }
}

void Session::lookAtWorld(float aspect) {
  const dunya::objectmodel::WorldExtent target =
    dunya::objectmodel::dynamicExtent(m_world);

  if (target.empty) {
    return;
  }

  const dunya::objectmodel::Framing framing =
    dunya::objectmodel::frameExtent(target);

  const dunya::objectmodel::Lens lens{};

  m_frame.proj = dunya::objectmodel::projection(lens, aspect);

  m_frame.view =
    glm::lookAt(framing.position, target.centre(), glm::vec3(0.0f, 1.0f, 0.0f));

  m_frame.cameraPos = glm::vec4(framing.position, lens.nearPlane);
}

void Session::resize() {
  const VkExtent2D current = m_windowSystem->framebufferExtent();

  if (current.width == 0 || current.height == 0) {
    return;
  }

  m_swapChain.recreate();
}

void Session::render() {
  const VkExtent2D current = m_windowSystem->framebufferExtent();

  if (current.width == 0 || current.height == 0) {
    return;
  }

  m_uploader.retire();

  lookAtWorld(
    static_cast<float>(m_swapChain.extent().width)
    / static_cast<float>(m_swapChain.extent().height)
  );

  m_framePacker.pack(
    m_frame,
    m_world,
    m_world.sdfGrids(),
    m_assetLibrary.meshBuffers()
  );

  if (m_renderer.drawFrame(m_swapChain, m_frame)) {
    m_swapChain.recreate();

    return;
  }

  for (const uint32_t slot : m_recordTable.bakeList()) {
    m_world.markBaked(m_framePacker.sdfRecordEntities()[slot]);
  }
}

void Session::retarget(std::unique_ptr<dunya::gpu::WindowSystem> windowSystem) {
  if (windowSystem == nullptr) {
    throw std::runtime_error("A session needs a window system");
  }

  m_swapChain.release();

  m_windowSystem = std::move(windowSystem);

  m_context.retarget(*m_windowSystem);

  m_swapChain.recreate();
}

VkExtent2D Session::extent() const noexcept {
  return m_swapChain.extent();
}

const dunya::objectmodel::World& Session::world() const noexcept {
  return m_world;
}

}
