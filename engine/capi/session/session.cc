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
    : m_engine(std::move(windowSystem), projectRoot),
      m_worldName(world),

      m_sceneTarget(
        m_engine.context().device(),
        m_engine.swapChain().imageFormat(),
        m_engine.swapChain().extent(),
        1.0f
      ),
      m_grid(
        m_engine.context().device(),
        m_engine.swapChain(),
        m_engine.setLayouts(dunya::gpu::PipelineType::Grid)
      ) {
  m_engine.renderer().useTarget(&m_sceneTarget);

  m_viewport = m_engine.createViewport();

  m_port.gridVisible = true;

  m_engine.loadWorld(world);
}

Session::~Session() {
  m_engine.context().device().waitIdle();

  try {
    stop();
  } catch (const std::exception& failure) {
    std::cerr << "Session teardown: " << failure.what() << std::endl;
  }
}

void Session::bindCamera() {
  m_port.mode = dunya::view::DrawMode::Both;

  if (m_viewsThroughScene) {
    m_port.camera = dunya::objectmodel::mainCamera(activeWorld());

    if (m_port.camera == dunya::objectmodel::INVALID_ENTITY) {
      m_port.mode = dunya::view::DrawMode::Nothing;
    }
  } else {
    if (!m_camera.placed()) {
      frameCameraOnWorld();
    }

    m_port.camera = dunya::objectmodel::INVALID_ENTITY;
    m_port.pose = m_camera.pose();
    m_port.lens = m_camera.lens();
  }

  if (!m_engine.configureViewport(m_viewport, m_port)) {
    throw std::runtime_error("the session's viewport went missing");
  }
}

void Session::setKey(uint32_t key, bool down) noexcept {
  m_engine.input().setKey(static_cast<dunya::systems::Key>(key), down);
}

void Session::setMouseButton(uint32_t button, bool down) noexcept {
  m_engine.input().setMouseButton(
    static_cast<dunya::systems::MouseButton>(button),
    down
  );
}

void Session::setCursor(float x, float y) noexcept {
  m_engine.input().setCursor(x, y);
}

bool Session::viewsThroughScene() const noexcept {
  return m_viewsThroughScene;
}

bool Session::package(
  const std::string& playerExecutable,
  const std::string& output,
  const std::vector<std::string>& worlds,
  std::string& result
) const {
  dunya::editor::PackageSpec spec{};
  spec.playerExecutable = playerExecutable;
  spec.projectRoot = m_engine.projectRoot();
  spec.output = output;
  spec.worlds = worlds.empty() ? std::vector<std::string>{m_worldName} : worlds;

  if (!dunya::editor::packageProject(spec, result)) {
    return false;
  }

  result = dunya::editor::packagedExecutable(spec).string();

  return true;
}

void Session::frameCameraOnWorld() {
  const dunya::objectmodel::WorldExtent whole =
    dunya::objectmodel::sceneExtent(m_engine.world());

  if (whole.empty) {
    m_camera.frame(glm::vec3(0.0f), 4.0f);

    return;
  }

  m_camera.frame(whole.centre(), 0.5f * glm::length(whole.span()));
}

void Session::alignToSceneCamera() {
  const dunya::objectmodel::Entity eye =
    dunya::objectmodel::firstLens(m_engine.world());

  if (eye == dunya::objectmodel::INVALID_ENTITY) {
    return;
  }

  const dunya::objectmodel::Pose& seat =
    m_engine.world().registry().get<dunya::objectmodel::Pose>(eye);

  const dunya::objectmodel::WorldExtent whole =
    dunya::objectmodel::sceneExtent(m_engine.world());

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
  const VkExtent2D size = m_engine.swapChain().extent();

  if (size.width == 0u || size.height == 0u) {
    return dunya::objectmodel::INVALID_ENTITY;
  }

  const glm::vec2 ndc(
    2.0f * x / static_cast<float>(size.width) - 1.0f,
    2.0f * y / static_cast<float>(size.height) - 1.0f
  );

  const dunya::objectmodel::CameraView seen = dunya::view::lookThrough(
    m_port,
    activeWorld(),
    static_cast<float>(size.width) / static_cast<float>(size.height)
  );

  const dunya::field::Ray ray = dunya::field::screenPointToRay(
    glm::inverse(seen.projection * seen.view),
    seen.position,
    ndc
  );

  return dunya::objectmodel::raycastWorld(activeWorld(), ray).entity;
}

void Session::resize() {
  const VkExtent2D current = m_engine.windowSystem().framebufferExtent();

  if (current.width == 0 || current.height == 0) {
    return;
  }

  m_engine.swapChain().recreate();

  m_sceneTarget.resize(m_engine.swapChain().extent());
}

void Session::render() {
  const VkExtent2D current = m_engine.windowSystem().framebufferExtent();

  if (current.width == 0 || current.height == 0) {
    return;
  }

  const auto frameAt = std::chrono::steady_clock::now();

  const float elapsed =
    std::chrono::duration<float>(frameAt - m_lastFrame).count();

  m_lastFrame = frameAt;

  bindCamera();

  std::vector<dunya::renderer::ScenePass> passes;

  if (m_port.gridVisible && !m_viewsThroughScene) {
    m_grid.update(m_camera.eye());

    passes.push_back(
      {dunya::renderer::PassOrder::AfterScene,
       [this](VkCommandBuffer commands) { drawGrid(commands); }}
    );
  }

  dunya::core::Telemetry ignored;

  dunya::engine::FrameRequest request;
  request.viewport = m_viewport;
  request.deltaSeconds = elapsed;
  request.passes = passes;

  if (m_engine.renderFrame(request, ignored)) {
    m_engine.swapChain().recreate();

    m_sceneTarget.resize(m_engine.swapChain().extent());
  }
}

void Session::retarget(std::unique_ptr<dunya::gpu::WindowSystem> windowSystem) {
  m_engine.retarget(std::move(windowSystem));

  m_sceneTarget.resize(m_engine.swapChain().extent());
}

VkExtent2D Session::extent() const noexcept {
  return m_engine.swapChain().extent();
}

const dunya::objectmodel::World& Session::world() const noexcept {
  return m_engine.world();
}

dunya::objectmodel::World& Session::world() noexcept {
  return m_engine.world();
}

dunya::systems::Schedule& Session::schedule() noexcept {
  return m_engine.schedule();
}

void Session::play() {
  if (m_engine.playing()) {
    return;
  }

  m_engine.play();

  m_viewsThroughScene = true;
}

void Session::stop() {
  if (!m_engine.playing()) {
    return;
  }

  m_engine.stop();

  m_viewsThroughScene = false;
}

bool Session::playing() const noexcept {
  return m_engine.playing();
}

dunya::objectmodel::World& Session::activeWorld() noexcept {
  return m_engine.activeWorld();
}

const dunya::objectmodel::World& Session::activeWorld() const noexcept {
  return m_engine.activeWorld();
}

dunya::objectmodel::Entity Session::createLight() {
  const dunya::objectmodel::Entity entity = m_engine.world().createAuthored();

  m_engine.world().emplaceOrReplace<dunya::objectmodel::Pose>(
    entity,
    dunya::objectmodel::Pose{}
  );
  m_engine.world().emplaceOrReplace<dunya::objectmodel::DirectionalLight>(
    entity,
    dunya::objectmodel::DirectionalLight{}
  );

  return entity;
}

dunya::objectmodel::Entity Session::createEnvironment() {
  const dunya::objectmodel::Entity entity = m_engine.world().createAuthored();

  m_engine.world().emplaceOrReplace<dunya::objectmodel::Pose>(
    entity,
    dunya::objectmodel::Pose{}
  );
  m_engine.world().emplaceOrReplace<dunya::objectmodel::Environment>(
    entity,
    dunya::objectmodel::Environment{}
  );

  return entity;
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

  const bool first = dunya::objectmodel::mainCamera(m_engine.world())
                     == dunya::objectmodel::INVALID_ENTITY;

  const dunya::objectmodel::Entity entity = m_engine.world().createAuthored();

  m_engine.world().emplaceOrReplace<dunya::objectmodel::Pose>(entity, seat);
  m_engine.world().emplaceOrReplace<dunya::objectmodel::Lens>(entity, lens);

  if (first && !m_engine.world().setMainCamera(entity)) {
    throw std::runtime_error("A new camera could not become the main camera");
  }

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

  return m_engine.world().createSdfGrid(pose, grid);
}

bool Session::addPrimitive(
  dunya::objectmodel::Entity entity,
  const dunya::field::Primitive& primitive
) {
  return m_engine.world().addPrimitive(entity, primitive);
}

void Session::setStatic(dunya::objectmodel::Entity entity) {
  m_engine.world().addStaticBody(entity);
}

void Session::setDeformable(dunya::objectmodel::Entity entity) {
  m_engine.world().emplaceOrReplace<dunya::objectmodel::Deformable>(
    entity,
    dunya::objectmodel::Deformable{}
  );
}

bool Session::destroyEntity(dunya::objectmodel::Entity entity) {
  return m_engine.world().destroy(entity);
}

bool Session::save() const {
  std::optional<dunya::serialize::Project> project =
    dunya::serialize::Project::open(m_engine.projectRoot());

  if (!project.has_value()) {
    return false;
  }

  return project->saveWorld(
    m_worldName,
    dunya::serialize::captureWorld(m_engine.world(), m_engine.assets().assets())
  );
}

size_t Session::materialCount() const noexcept {
  return m_engine.assets().assets().of<dunya::objectmodel::Material>().size();
}

dunya::core::AssetId Session::materialAt(uint32_t index) const noexcept {
  return m_engine.assets().assets().id<dunya::objectmodel::Material>(index);
}

bool Session::openWorld(const std::string& name) {
  std::optional<dunya::serialize::Project> project =
    dunya::serialize::Project::open(m_engine.projectRoot());

  if (!project.has_value()) {
    return false;
  }

  dunya::serialize::StoredWorld stored;

  if (!project->loadWorld(name, stored)) {
    return false;
  }

  stop();

  m_engine.context().device().waitIdle();

  m_engine.storage().residency().releaseAll(m_engine.world());

  m_engine.world().clear();

  if (!dunya::serialize::restoreWorld(
        stored,
        m_engine.world(),
        m_engine.assets().assets()
      )) {
    return false;
  }

  m_worldName = name;
  m_camera.reset();

  return true;
}

bool Session::newWorld(const std::string& name) {
  std::optional<dunya::serialize::Project> project =
    dunya::serialize::Project::open(m_engine.projectRoot());

  if (!project.has_value()) {
    return false;
  }

  dunya::objectmodel::World fresh;

  dunya::objectmodel::addDefaultEntities(fresh);

  if (!project->saveWorld(
        name,
        dunya::serialize::captureWorld(fresh, m_engine.assets().assets())
      )) {
    return false;
  }

  return openWorld(name);
}

bool Session::saveAs(const std::string& name) {
  std::optional<dunya::serialize::Project> project =
    dunya::serialize::Project::open(m_engine.projectRoot());

  if (!project.has_value()) {
    return false;
  }

  if (!project->saveWorld(
        name,
        dunya::serialize::captureWorld(
          m_engine.world(),
          m_engine.assets().assets()
        )
      )) {
    return false;
  }

  m_worldName = name;

  return true;
}

std::string Session::worldNames() const {
  const std::filesystem::path folder =
    m_engine.projectRoot() / dunya::serialize::WORLD_FOLDER;

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
    dunya::serialize::Project::open(m_engine.projectRoot());

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
    dunya::serialize::Project::open(m_engine.projectRoot());

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

dunya::view::Viewport Session::viewSettings() const noexcept {
  return m_port;
}

void Session::setViewSettings(const dunya::view::Viewport& settings) {
  const bool scaleChanged = settings.supersample != m_port.supersample;

  m_port.gridVisible = settings.gridVisible;
  m_port.supersample = settings.supersample;
  m_port.mode = settings.mode;
  m_port.fieldRepresentation = settings.fieldRepresentation;
  m_port.march = settings.march;

  if (scaleChanged) {
    m_engine.context().device().waitIdle();

    m_sceneTarget.setScale(m_port.supersample);
  }
}

void Session::drawGrid(VkCommandBuffer commands) const {
  m_grid.record(commands, m_engine.globals());
}

const std::string& Session::worldName() const noexcept {
  return m_worldName;
}

uint32_t Session::materialIndex(dunya::core::AssetId id) const noexcept {
  return m_engine.assets().assets().index<dunya::objectmodel::Material>(id);
}

dunya::core::AssetId Session::addMaterial(
  const glm::vec4& baseColor,
  float metallic,
  float roughness
) {
  std::optional<dunya::serialize::Project> project =
    dunya::serialize::Project::open(m_engine.projectRoot());

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

  static_cast<void>(m_engine.assets().addMaterial(minted, stored));

  m_engine.context().device().waitIdle();
  m_engine.storage().resourceTable().refresh(m_engine.assets().materials());

  return minted;
}

}
