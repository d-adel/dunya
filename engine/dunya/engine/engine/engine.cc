#include "engine.ih"

namespace dunya::engine {

namespace {

constexpr uint32_t MAX_PHYSICS_SUBSTEPS = 5;

constexpr double MAX_PHYSICS_DELTA = MAX_PHYSICS_SUBSTEPS / 60.0;

dunya::gpu::WindowSystem& require(
  const std::unique_ptr<dunya::gpu::WindowSystem>& windowSystem
) {
  if (windowSystem == nullptr) {
    throw std::runtime_error("An engine needs a window system");
  }

  return *windowSystem;
}

const std::vector<VkVertexInputBindingDescription> MESH_BINDINGS{
  dunya::renderer::Vertex::getBindingDescription()
};

const auto MESH_ATTRIBUTES =
  dunya::renderer::Vertex::getAttributeDescriptions();

}

const dunya::script::PhysicsVerbs Engine::PHYSICS_VERBS{
  &Engine::onSetRigidBody,
  &Engine::onSetVelocity,
  &Engine::onDestroy
};

Engine::Engine(
  std::unique_ptr<dunya::gpu::WindowSystem> windowSystem,
  const std::filesystem::path& projectRoot
)
    : m_windowSystem(std::move(windowSystem)),
      m_context(require(m_windowSystem)),
      m_swapChain(m_context),
      m_projectRoot(projectRoot),
      m_assetLibrary(m_context, projectRoot),
      m_storage(
        m_context.device(),
        m_assetLibrary.textures(),
        m_assetLibrary.samplers(),
        m_assetLibrary.materials()
      ),
      m_meshPipeline(
        dunya::gpu::PipelineType::Mesh,
        m_context.device().vkDevice(),
        setLayouts(dunya::gpu::PipelineType::Mesh),
        m_swapChain,
        MESH_BINDINGS,
        MESH_ATTRIBUTES
      ),
      m_sdfPipeline(
        dunya::gpu::PipelineType::Sdf,
        m_context.device().vkDevice(),
        setLayouts(dunya::gpu::PipelineType::Sdf),
        m_swapChain
      ),
      m_skyPipeline(
        dunya::gpu::PipelineType::Sky,
        m_context.device().vkDevice(),
        setLayouts(dunya::gpu::PipelineType::Sky),
        m_swapChain
      ),
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
  m_storage.residency().attach(m_world);
}

Engine::~Engine() {
  try {
    stop();
  } catch (const std::exception& failure) {
    std::cerr << "Engine teardown: " << failure.what() << std::endl;
  }
}

std::vector<VkDescriptorSetLayout> Engine::setLayouts(
  dunya::gpu::PipelineType type
) {
  return dunya::renderer::pipelineSetLayouts(
    type,
    m_storage.frameGlobals(),
    m_storage.resourceTable(),
    m_storage.recordTable()
  );
}

const std::filesystem::path& Engine::projectRoot() const noexcept {
  return m_projectRoot;
}

void Engine::loadWorld(const std::string& world) {
  std::optional<dunya::serialize::Project> project =
    dunya::serialize::Project::open(m_projectRoot);

  if (!project.has_value()) {
    throw std::runtime_error("No project at " + m_projectRoot.string());
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

void Engine::play() {
  if (m_runtime) {
    return;
  }

  m_storage.residency().releaseAll(m_world);

  m_runtime.emplace(m_world, m_jolt);

  m_storage.residency().attach(m_runtime->world());

  m_physicsScope = dunya::script::PhysicsScope(&Engine::PHYSICS_VERBS, this);
}

void Engine::stop() {
  if (!m_runtime) {
    return;
  }

  m_storage.residency().releaseAll(m_runtime->world());

  m_physicsScope = {};

  m_pendingBodies.clear();
  m_accumulator = 0.0;

  m_runtime.reset();
}

bool Engine::playing() const noexcept {
  return m_runtime.has_value();
}

dunya::objectmodel::World& Engine::world() noexcept {
  return m_world;
}

const dunya::objectmodel::World& Engine::world() const noexcept {
  return m_world;
}

dunya::objectmodel::World& Engine::activeWorld() noexcept {
  return m_runtime ? m_runtime->world() : m_world;
}

const dunya::objectmodel::World& Engine::activeWorld() const noexcept {
  return m_runtime ? m_runtime->world() : m_world;
}

dunya::assets::AssetLibrary& Engine::assets() noexcept {
  return m_assetLibrary;
}

const dunya::assets::AssetLibrary& Engine::assets() const noexcept {
  return m_assetLibrary;
}

dunya::gpu::Context& Engine::context() noexcept {
  return m_context;
}

const dunya::gpu::Context& Engine::context() const noexcept {
  return m_context;
}

dunya::gpu::SwapChain& Engine::swapChain() noexcept {
  return m_swapChain;
}

const dunya::gpu::SwapChain& Engine::swapChain() const noexcept {
  return m_swapChain;
}

dunya::gpu::WindowSystem& Engine::windowSystem() noexcept {
  return require(m_windowSystem);
}

dunya::renderer::RendererStorage& Engine::storage() noexcept {
  return m_storage;
}

const dunya::renderer::RendererStorage& Engine::storage() const noexcept {
  return m_storage;
}

dunya::renderer::Renderer& Engine::renderer() noexcept {
  return m_renderer;
}

const dunya::renderer::Renderer& Engine::renderer() const noexcept {
  return m_renderer;
}

dunya::renderer::Frame& Engine::frame() noexcept {
  return m_frame;
}

const dunya::renderer::Frame& Engine::frame() const noexcept {
  return m_frame;
}

dunya::gpu::Pipeline& Engine::meshPipeline() noexcept {
  return m_meshPipeline;
}

dunya::gpu::Pipeline& Engine::sdfPipeline() noexcept {
  return m_sdfPipeline;
}

dunya::systems::Schedule& Engine::schedule() noexcept {
  return m_schedule;
}

dunya::systems::InputState& Engine::input() noexcept {
  return m_input;
}

const dunya::systems::InputState& Engine::input() const noexcept {
  return m_input;
}

dunya::runtime::Runtime* Engine::runtime() noexcept {
  return m_runtime ? &*m_runtime : nullptr;
}

uint32_t Engine::frameIndex() const noexcept {
  return m_frameIndex;
}

void Engine::stepPhysics(
  float deltaSeconds,
  dunya::core::Telemetry& telemetry
) {
  if (!m_runtime) {
    return;
  }

  m_accumulator += std::min(double(deltaSeconds), MAX_PHYSICS_DELTA);

  uint32_t substeps = 0;

  const auto started = std::chrono::steady_clock::now();

  while (m_accumulator >= dunya::physics::PhysicsWorld::TIME_STEP
         && substeps != MAX_PHYSICS_SUBSTEPS) {
    ++substeps;
    m_runtime->step();

    m_accumulator -= dunya::physics::PhysicsWorld::TIME_STEP;
  }

  telemetry.set(
    telemetry.key("physics"),
    std::chrono::duration<double, std::milli>(
      std::chrono::steady_clock::now() - started
    )
      .count()
  );

  telemetry.set(telemetry.key("substeps"), double(substeps));

  telemetry.set(
    telemetry.key("awake"),
    double(m_runtime->physics().system().GetNumActiveBodies(
      JPH::EBodyType::RigidBody
    ))
  );

  {
    const auto carveStart = std::chrono::steady_clock::now();

    m_runtime->applyImpacts();

    telemetry.set(
      telemetry.key("carve"),
      std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - carveStart
      )
        .count()
    );

    telemetry.set(
      telemetry.key("craters"),
      double(m_runtime->cratersThisFrame().size())
    );
  }
}

void Engine::tick(float deltaSeconds, dunya::core::Telemetry& telemetry) {
  ++m_frameIndex;

  if (!m_runtime) {
    return;
  }

  stepPhysics(deltaSeconds, telemetry);

  runSystems(deltaSeconds);

  applyPendingBodies();

  m_runtime->refreshDeformedBodies();

  m_runtime->syncPoses(
    float(m_accumulator / dunya::physics::PhysicsWorld::TIME_STEP)
  );
}

void Engine::runSystems(float deltaSeconds) {
  dunya::systems::Context context{
    activeWorld(),
    m_input,
    deltaSeconds,
    m_frameIndex
  };

  m_schedule.run(context);
}

void Engine::flushVolumes(dunya::core::Telemetry& telemetry) {
  m_storage.residency().flush(activeWorld(), telemetry);
}

void Engine::packFrame(bool holdBakes) {
  m_frame.environment.reset();
  m_frame.light.reset();

  m_storage.framePacker().pack(
    m_frame,
    activeWorld(),
    holdBakes ? std::span<const dunya::objectmodel::Entity>()
              : activeWorld().sdfGrids(),
    m_assetLibrary.meshBuffers(),
    [this](dunya::objectmodel::Entity entity) {
      if (m_runtime) {
        m_runtime->refreshBody(entity);
      }
    }
  );
}

void Engine::endFrame() noexcept {
  m_input.beginFrame();
}

void Engine::drawSky(VkCommandBuffer commands) const {
  vkCmdBindPipeline(
    commands,
    VK_PIPELINE_BIND_POINT_GRAPHICS,
    m_skyPipeline.pipeline()
  );

  const VkDescriptorSet set = globals();

  vkCmdBindDescriptorSets(
    commands,
    VK_PIPELINE_BIND_POINT_GRAPHICS,
    m_skyPipeline.pipelineLayout(),
    0,
    1,
    &set,
    0,
    nullptr
  );

  vkCmdDraw(commands, 3, 1, 0, 0);
}

VkDescriptorSet Engine::globals() const noexcept {
  return m_storage.frameGlobals().descriptorSet(m_renderer.currentFrame());
}

void Engine::resize() {
  m_swapChain.recreate();
}

void Engine::retarget(std::unique_ptr<dunya::gpu::WindowSystem> windowSystem) {
  if (windowSystem == nullptr) {
    throw std::runtime_error("An engine needs a window system");
  }

  m_swapChain.release();

  m_windowSystem = std::move(windowSystem);

  m_context.retarget(*m_windowSystem);

  m_swapChain.recreate();
}

int32_t Engine::onSetRigidBody(void* host, void*, uint32_t entity, float mass) {
  auto* live = static_cast<Engine*>(host);

  if (live == nullptr || !live->m_runtime) {
    return 0;
  }

  const auto subject =
    static_cast<dunya::objectmodel::Entity>(entt::entity{entity});

  if (!live->m_runtime->world().registry().valid(subject)) {
    return 0;
  }

  live->remember(subject).mass = mass;

  return 1;
}

int32_t Engine::onSetVelocity(
  void* host,
  void*,
  uint32_t entity,
  const float* velocity
) {
  auto* live = static_cast<Engine*>(host);

  if (live == nullptr || velocity == nullptr || !live->m_runtime) {
    return 0;
  }

  const auto subject =
    static_cast<dunya::objectmodel::Entity>(entt::entity{entity});

  if (!live->m_runtime->world().registry().valid(subject)) {
    return 0;
  }

  live->remember(subject).velocity =
    glm::vec3(velocity[0], velocity[1], velocity[2]);

  return 1;
}

int32_t Engine::onDestroy(void* host, void*, uint32_t entity) {
  auto* live = static_cast<Engine*>(host);

  if (live == nullptr || !live->m_runtime) {
    return 0;
  }

  const auto subject =
    static_cast<dunya::objectmodel::Entity>(entt::entity{entity});

  std::erase_if(live->m_pendingBodies, [subject](const PendingBody& waiting) {
    return waiting.entity == subject;
  });

  return live->m_runtime->destroy(subject) ? 1 : 0;
}

Engine::PendingBody& Engine::remember(dunya::objectmodel::Entity entity) {
  for (PendingBody& waiting : m_pendingBodies) {
    if (waiting.entity == entity) {
      return waiting;
    }
  }

  m_pendingBodies.push_back({entity, std::nullopt, std::nullopt});

  return m_pendingBodies.back();
}

void Engine::applyPendingBodies() {
  if (!m_runtime || m_pendingBodies.empty()) {
    return;
  }

  std::vector<PendingBody> waiting;

  for (const PendingBody& body : m_pendingBodies) {
    if (!m_runtime->world().registry().valid(body.entity)) {
      continue;
    }

    if (!m_runtime->world().hasSampledSdf(body.entity)) {
      waiting.push_back(body);

      continue;
    }

    m_runtime->refreshBody(body.entity);

    if (body.mass.has_value()) {
      m_runtime->setMass(body.entity, *body.mass);
    }

    if (body.velocity.has_value()) {
      m_runtime->setLinearVelocity(body.entity, *body.velocity);
    }
  }

  m_pendingBodies = std::move(waiting);
}

}
