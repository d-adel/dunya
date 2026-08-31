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
      m_projectRoot(projectRoot),
      m_worldName(world),
      m_storage(
        m_context.device(),
        m_assetLibrary.textures(),
        m_assetLibrary.samplers(),
        m_assetLibrary.materials()
      ),
      m_meshPipeline(
        dunya::gpu::PipelineType::Mesh,
        m_context.device().vkDevice(),
        dunya::renderer::pipelineSetLayouts(
          dunya::gpu::PipelineType::Mesh,
          m_storage.frameGlobals(),
          m_storage.resourceTable(),
          m_storage.recordTable()
        ),
        m_swapChain,
        std::vector<VkVertexInputBindingDescription>{
          dunya::renderer::Vertex::getBindingDescription()
        },
        dunya::renderer::Vertex::getAttributeDescriptions()
      ),
      m_sdfPipeline(
        dunya::gpu::PipelineType::Sdf,
        m_context.device().vkDevice(),
        dunya::renderer::pipelineSetLayouts(
          dunya::gpu::PipelineType::Sdf,
          m_storage.frameGlobals(),
          m_storage.resourceTable(),
          m_storage.recordTable()
        ),
        m_swapChain
      ),
      m_gridPipeline(
        dunya::gpu::PipelineType::Grid,
        m_context.device().vkDevice(),
        dunya::renderer::pipelineSetLayouts(
          dunya::gpu::PipelineType::Grid,
          m_storage.frameGlobals(),
          m_storage.resourceTable(),
          m_storage.recordTable()
        ),
        m_swapChain,
        std::vector<VkVertexInputBindingDescription>{
          dunya::viewport::GridVertex::bindingDescription()
        },
        dunya::viewport::GridVertex::attributeDescriptions()
      ),
      m_skyPipeline(
        dunya::gpu::PipelineType::Sky,
        m_context.device().vkDevice(),
        dunya::renderer::pipelineSetLayouts(
          dunya::gpu::PipelineType::Sky,
          m_storage.frameGlobals(),
          m_storage.resourceTable(),
          m_storage.recordTable()
        ),
        m_swapChain
      ),
      m_sceneTarget(
        m_context.device(),
        m_swapChain.imageFormat(),
        m_swapChain.extent(),
        2.0f
      ),
      m_grid(m_context.device()),
      m_renderer(
        m_context.device(),
        m_storage.recordTable(),
        m_storage.sdfBaker(),
        m_storage.volumePool(),
        m_storage.frameGlobals(),
        m_meshPipeline,
        m_sdfPipeline,
        m_storage.resourceTable(),
        m_context.surface().handle(),
        m_swapChain.imageCount()
      ) {
  m_renderer.useTarget(&m_sceneTarget);

  m_frame.environment = dunya::objectmodel::Environment{};

  loadWorld(projectRoot, world);
}

Session::~Session() {
  m_context.device().waitIdle();

  try {
    stop();
  } catch (const std::exception& failure) {
    std::cerr << "Session teardown: " << failure.what() << std::endl;
  }
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
  if (!m_camera.placed()) {
    frameCameraOnWorld();
  }

  m_frame.proj = dunya::objectmodel::projection(m_camera.lens(), aspect);
  m_frame.view = m_camera.viewMatrix();
  m_frame.cameraPos = glm::vec4(m_camera.eye(), m_camera.lens().nearPlane);
}

void Session::frameCameraOnWorld() {
  const dunya::objectmodel::WorldExtent whole =
    dunya::objectmodel::dynamicExtent(m_world);

  if (whole.empty) {
    m_camera.frame(glm::vec3(0.0f), 4.0f);

    return;
  }

  m_camera.frame(whole.centre(), 0.5f * glm::length(whole.span()));
}

void Session::alignToSceneCamera() {
  const dunya::objectmodel::Entity eye = dunya::objectmodel::firstLens(m_world);

  if (eye == dunya::objectmodel::INVALID_ENTITY) {
    return;
  }

  const dunya::objectmodel::Pose& seat =
    m_world.registry().get<dunya::objectmodel::Pose>(eye);

  const dunya::objectmodel::WorldExtent whole =
    dunya::objectmodel::dynamicExtent(m_world);

  const float reach =
    whole.empty ? 10.0f : glm::length(whole.centre() - seat.position);

  m_camera.placeFrom(seat, glm::max(reach, 1.0f));
}

void Session::orbitCamera(float deltaYaw, float deltaPitch) {
  m_camera.orbit(deltaYaw, deltaPitch);
}

void Session::panCamera(float deltaX, float deltaY) {
  m_camera.pan(deltaX, deltaY);
}

void Session::zoomCamera(float delta) {
  m_camera.zoom(delta);
}

void Session::focusCamera(dunya::objectmodel::Entity entity) {
  if (entity == dunya::objectmodel::INVALID_ENTITY) {
    frameCameraOnWorld();

    return;
  }

  const dunya::objectmodel::WorldExtent around =
    dunya::objectmodel::entityExtent(activeWorld(), entity);

  if (around.empty) {
    return;
  }

  m_camera.frame(around.centre(), 0.5f * glm::length(around.span()));
}

dunya::objectmodel::Entity Session::pick(float x, float y) {
  const VkExtent2D size = m_swapChain.extent();

  if (size.width == 0u || size.height == 0u) {
    return dunya::objectmodel::INVALID_ENTITY;
  }

  const glm::vec2 ndc(
    2.0f * x / static_cast<float>(size.width) - 1.0f,
    2.0f * y / static_cast<float>(size.height) - 1.0f
  );

  const glm::mat4 inverseViewProjection =
    glm::inverse(m_frame.proj * m_frame.view);

  const glm::vec4 near = inverseViewProjection * glm::vec4(ndc, 0.0f, 1.0f);
  const glm::vec4 far = inverseViewProjection * glm::vec4(ndc, 1.0f, 1.0f);

  if (near.w == 0.0f || far.w == 0.0f) {
    return dunya::objectmodel::INVALID_ENTITY;
  }

  const glm::vec3 from = glm::vec3(near) / near.w;
  const glm::vec3 to = glm::vec3(far) / far.w;

  const dunya::field::Ray ray{from, glm::normalize(to - from)};

  return dunya::objectmodel::raycastWorld(activeWorld(), ray).entity;
}

void Session::resize() {
  const VkExtent2D current = m_windowSystem->framebufferExtent();

  if (current.width == 0 || current.height == 0) {
    return;
  }

  m_swapChain.recreate();

  m_sceneTarget.resize(m_swapChain.extent());
}

void Session::render() {
  const VkExtent2D current = m_windowSystem->framebufferExtent();

  if (current.width == 0 || current.height == 0) {
    return;
  }

  m_storage.uploader().retire();

  lookAtWorld(
    static_cast<float>(m_swapChain.extent().width)
    / static_cast<float>(m_swapChain.extent().height)
  );

  ++m_frameIndex;

  if (m_runtime) {
    m_runtime->step();
    m_runtime->syncPoses(1.0f);
  }

  dunya::systems::Context systemContext{
    activeWorld(),
    1.0f / 60.0f,
    m_frameIndex
  };
  m_schedule.run(systemContext);

  dunya::core::Telemetry ignored;

  m_storage.residency().flush(activeWorld(), ignored);

  m_storage.framePacker().pack(
    m_frame,
    activeWorld(),
    activeWorld().sdfGrids(),
    m_assetLibrary.meshBuffers(),
    [this](dunya::objectmodel::Entity entity) {
      if (m_runtime) {
        m_runtime->refreshBody(entity);
      }
    }
  );

  m_grid.update(m_camera.eye());

  const std::array<dunya::renderer::ScenePass, 2> passes{
    dunya::renderer::ScenePass{
      dunya::renderer::PassOrder::BeforeScene,
      [this](VkCommandBuffer commands) { drawSky(commands); }
    },
    dunya::renderer::ScenePass{
      dunya::renderer::PassOrder::AfterScene,
      [this](VkCommandBuffer commands) { drawGrid(commands); }
    }
  };

  if (m_renderer.drawFrame(m_swapChain, m_frame, passes)) {
    m_swapChain.recreate();

    m_sceneTarget.resize(m_swapChain.extent());

    return;
  }

  m_storage.framePacker().commitBakes();
}

void Session::retarget(std::unique_ptr<dunya::gpu::WindowSystem> windowSystem) {
  if (windowSystem == nullptr) {
    throw std::runtime_error("A session needs a window system");
  }

  m_swapChain.release();

  m_windowSystem = std::move(windowSystem);

  m_context.retarget(*m_windowSystem);

  m_swapChain.recreate();

  m_sceneTarget.resize(m_swapChain.extent());
}

VkExtent2D Session::extent() const noexcept {
  return m_swapChain.extent();
}

const dunya::objectmodel::World& Session::world() const noexcept {
  return m_world;
}

dunya::objectmodel::World& Session::world() noexcept {
  return m_world;
}

dunya::systems::Schedule& Session::schedule() noexcept {
  return m_schedule;
}

void Session::onDeform(
  void* host,
  uint32_t entity,
  const dunya::script::SdfDeformSummary* summary
) {
  auto* session = static_cast<Session*>(host);

  if (session == nullptr || summary == nullptr || !session->m_runtime) {
    return;
  }

  const auto subject =
    static_cast<dunya::objectmodel::Entity>(entt::entity{entity});

  session->m_runtime->reshapeAfterDeform(
    subject,
    glm::uvec3(
      summary->brickBegin[0],
      summary->brickBegin[1],
      summary->brickBegin[2]
    ),
    glm::uvec3(summary->brickEnd[0], summary->brickEnd[1], summary->brickEnd[2])
  );

  dunya::field::SampleBox touched{};
  touched.minimum = glm::uvec3(
    summary->sampleMinimum[0],
    summary->sampleMinimum[1],
    summary->sampleMinimum[2]
  );
  touched.extent = glm::uvec3(
    summary->sampleExtent[0],
    summary->sampleExtent[1],
    summary->sampleExtent[2]
  );

  session->activeWorld().markSdfDirty(subject, touched);

  dunya::objectmodel::World& world = session->m_runtime->world();

  if (world.registry().all_of<dunya::objectmodel::Pose>(subject)) {
    const glm::vec3 at = glm::vec3(
      dunya::objectmodel::model(
        world.registry().get<dunya::objectmodel::Pose>(subject)
      )[3]
    );

    session->m_runtime->wake(at - glm::vec3(8.0f), at + glm::vec3(8.0f));
  }
}

void Session::play() {
  if (m_runtime) {
    return;
  }

  m_storage.residency().releaseAll(m_world);

  m_runtime.emplace(m_world, m_jolt);

  m_deformScope = dunya::script::SdfDeformScope(&Session::onDeform, this);
}

void Session::stop() {
  if (!m_runtime) {
    return;
  }

  m_storage.residency().releaseAll(m_runtime->world());

  m_deformScope = {};

  m_runtime.reset();
}

bool Session::playing() const noexcept {
  return m_runtime.has_value();
}

dunya::objectmodel::World& Session::activeWorld() noexcept {
  return m_runtime ? m_runtime->world() : m_world;
}

const dunya::objectmodel::World& Session::activeWorld() const noexcept {
  return m_runtime ? m_runtime->world() : m_world;
}

dunya::objectmodel::Entity Session::createCamera(
  const glm::vec3& position,
  const glm::vec3& target,
  float verticalFov
) {
  const glm::vec3 ahead = glm::normalize(target - position);

  const float yaw = std::atan2(ahead.x, -ahead.z);
  const float pitch = std::asin(glm::clamp(ahead.y, -1.0f, 1.0f));

  dunya::objectmodel::Pose seat{};
  seat.position = position;
  seat.rotation = glm::angleAxis(yaw, glm::vec3(0.0f, -1.0f, 0.0f))
                  * glm::angleAxis(pitch, glm::vec3(1.0f, 0.0f, 0.0f));

  dunya::objectmodel::Lens lens{};
  lens.verticalFov = verticalFov;

  const dunya::objectmodel::Entity entity = m_world.createAuthored();

  m_world.emplaceOrReplace<dunya::objectmodel::Pose>(entity, seat);
  m_world.emplaceOrReplace<dunya::objectmodel::Lens>(entity, lens);

  return entity;
}

dunya::objectmodel::Entity Session::createSdf(
  const dunya::objectmodel::Pose& pose,
  const glm::uvec3& resolution,
  float margin
) {
  dunya::objectmodel::SdfGrid grid{};
  grid.resolution = resolution;
  grid.margin = margin;

  return m_world.createSdfGrid(pose, grid);
}

bool Session::addPrimitive(
  dunya::objectmodel::Entity entity,
  const dunya::field::Primitive& primitive
) {
  return m_world.addPrimitive(entity, primitive);
}

void Session::setStatic(dunya::objectmodel::Entity entity) {
  m_world.addStaticBody(entity);
}

void Session::setDeformable(dunya::objectmodel::Entity entity) {
  m_world.emplaceOrReplace<dunya::objectmodel::Deformable>(
    entity,
    dunya::objectmodel::Deformable{}
  );
}

bool Session::destroyEntity(dunya::objectmodel::Entity entity) {
  return m_world.destroy(entity);
}

bool Session::save() const {
  std::optional<dunya::serialize::Project> project =
    dunya::serialize::Project::open(m_projectRoot);

  if (!project.has_value()) {
    return false;
  }

  return project->saveWorld(
    m_worldName,
    dunya::serialize::captureWorld(m_world, m_assetLibrary.assets())
  );
}

size_t Session::materialCount() const noexcept {
  return m_assetLibrary.assets().of<dunya::objectmodel::Material>().size();
}

dunya::core::AssetId Session::materialAt(uint32_t index) const noexcept {
  return m_assetLibrary.assets().id<dunya::objectmodel::Material>(index);
}

bool Session::openWorld(const std::string& name) {
  std::optional<dunya::serialize::Project> project =
    dunya::serialize::Project::open(m_projectRoot);

  if (!project.has_value()) {
    return false;
  }

  dunya::serialize::StoredWorld stored;

  if (!project->loadWorld(name, stored)) {
    return false;
  }

  stop();

  m_context.device().waitIdle();

  m_storage.residency().releaseAll(m_world);

  m_world.clear();

  if (!dunya::serialize::restoreWorld(
        stored,
        m_world,
        m_assetLibrary.assets()
      )) {
    return false;
  }

  m_worldName = name;
  m_camera.reset();

  return true;
}

bool Session::newWorld(const std::string& name) {
  std::optional<dunya::serialize::Project> project =
    dunya::serialize::Project::open(m_projectRoot);

  if (!project.has_value()) {
    return false;
  }

  if (!project->saveWorld(name, dunya::serialize::StoredWorld{})) {
    return false;
  }

  return openWorld(name);
}

bool Session::saveAs(const std::string& name) {
  std::optional<dunya::serialize::Project> project =
    dunya::serialize::Project::open(m_projectRoot);

  if (!project.has_value()) {
    return false;
  }

  if (!project->saveWorld(
        name,
        dunya::serialize::captureWorld(m_world, m_assetLibrary.assets())
      )) {
    return false;
  }

  m_worldName = name;

  return true;
}

std::string Session::worldNames() const {
  const std::filesystem::path folder =
    m_projectRoot / dunya::serialize::WORLD_FOLDER;

  std::vector<std::string> names;

  std::error_code ignored;

  for (const std::filesystem::directory_entry& entry :
       std::filesystem::directory_iterator(folder, ignored)) {
    const std::string file = entry.path().filename().string();

    const size_t suffix = file.rfind(".world.json");

    if (suffix != std::string::npos && suffix + 11u == file.size()) {
      names.push_back(file.substr(0, suffix));
    }
  }

  std::ranges::sort(names);

  std::string joined;

  for (const std::string& name : names) {
    if (!joined.empty()) {
      joined.push_back('\n');
    }

    joined += name;
  }

  return joined;
}

std::string Session::assetLines() const {
  std::optional<dunya::serialize::Project> project =
    dunya::serialize::Project::open(m_projectRoot);

  if (!project.has_value()) {
    return {};
  }

  std::string joined;

  for (const dunya::serialize::AssetEntry& entry : project->manifest().assets) {
    if (!joined.empty()) {
      joined.push_back('\n');
    }

    joined += std::to_string(entry.id) + '\t' + entry.type + '\t' + entry.path;
  }

  return joined;
}

dunya::core::AssetId Session::importAsset(
  const std::filesystem::path& file,
  const std::string& type
) {
  std::optional<dunya::serialize::Project> project =
    dunya::serialize::Project::open(m_projectRoot);

  if (!project.has_value()) {
    throw std::runtime_error("The session has no project to import into");
  }

  const dunya::core::AssetId minted = project->importAsset(file, type);

  if (minted == dunya::core::INVALID_ASSET) {
    throw std::runtime_error("The asset could not be imported");
  }

  if (!project->save()) {
    throw std::runtime_error("The project manifest could not be saved");
  }

  return minted;
}

void Session::setSupersample(float scale) {
  m_context.device().waitIdle();

  m_sceneTarget.setScale(scale);
}

float Session::supersample() const noexcept {
  return m_sceneTarget.scale();
}

void Session::drawSky(VkCommandBuffer commands) const {
  vkCmdBindPipeline(
    commands,
    VK_PIPELINE_BIND_POINT_GRAPHICS,
    m_skyPipeline.pipeline()
  );

  const VkDescriptorSet& globals =
    m_storage.frameGlobals().descriptorSet(m_renderer.currentFrame());

  vkCmdBindDescriptorSets(
    commands,
    VK_PIPELINE_BIND_POINT_GRAPHICS,
    m_skyPipeline.pipelineLayout(),
    0,
    1,
    &globals,
    0,
    nullptr
  );

  vkCmdDraw(commands, 3, 1, 0, 0);
}

void Session::showGrid(bool visible) noexcept {
  m_gridVisible = visible;
}

bool Session::gridVisible() const noexcept {
  return m_gridVisible;
}

void Session::drawGrid(VkCommandBuffer commands) const {
  if (!m_gridVisible) {
    return;
  }

  vkCmdBindPipeline(
    commands,
    VK_PIPELINE_BIND_POINT_GRAPHICS,
    m_gridPipeline.pipeline()
  );

  const VkDescriptorSet& globals =
    m_storage.frameGlobals().descriptorSet(m_renderer.currentFrame());

  vkCmdBindDescriptorSets(
    commands,
    VK_PIPELINE_BIND_POINT_GRAPHICS,
    m_gridPipeline.pipelineLayout(),
    0,
    1,
    &globals,
    0,
    nullptr
  );

  m_grid.draw(commands, m_gridPipeline.pipelineLayout());
}

const std::string& Session::worldName() const noexcept {
  return m_worldName;
}

uint32_t Session::materialIndex(dunya::core::AssetId id) const noexcept {
  return m_assetLibrary.assets().index<dunya::objectmodel::Material>(id);
}

dunya::core::AssetId Session::addMaterial(
  const glm::vec4& baseColor,
  float metallic,
  float roughness
) {
  std::optional<dunya::serialize::Project> project =
    dunya::serialize::Project::open(m_projectRoot);

  if (!project.has_value()) {
    throw std::runtime_error("The session has no project to add a material to");
  }

  dunya::serialize::StoredMaterial stored{};
  stored.baseColor = baseColor;
  stored.metallic = metallic;
  stored.roughness = roughness;

  const std::filesystem::path staged =
    std::filesystem::temp_directory_path()
    / ("material-" + std::to_string(materialCount()) + ".mat.json");

  if (!dunya::serialize::writeText(
        staged,
        dunya::serialize::writeMaterial(stored)
      )) {
    throw std::runtime_error("The material could not be written");
  }

  const dunya::core::AssetId minted = project->importAsset(staged, "material");

  std::error_code ignored;
  std::filesystem::remove(staged, ignored);

  if (minted == dunya::core::INVALID_ASSET) {
    throw std::runtime_error("The material could not be registered");
  }

  if (!project->save()) {
    throw std::runtime_error("The project manifest could not be saved");
  }

  static_cast<void>(m_assetLibrary.addMaterial(minted, stored));

  m_context.device().waitIdle();
  m_storage.resourceTable().refresh(m_assetLibrary.materials());

  return minted;
}

}
