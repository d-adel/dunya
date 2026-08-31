#include "application.ih"

namespace {

constexpr uint32_t MAX_PHYSICS_SUBSTEPS = 5;

constexpr double MAX_PHYSICS_DELTA = MAX_PHYSICS_SUBSTEPS / 60.0;

constexpr size_t MAX_BALLS = 24;

constexpr float MUZZLE_DISTANCE = 2.5f;

const std::vector<VkVertexInputBindingDescription> MESH_BINDINGS{
  dunya::renderer::Vertex::getBindingDescription()
};

const auto MESH_ATTRIBUTES =
  dunya::renderer::Vertex::getAttributeDescriptions();

}

Application::Application(
  const StartupOptions& options,
  DebugUiFactory tools,
  ViewSource viewSource
)
    : m_windowSystem(m_window),
      m_context(m_windowSystem),
      m_input(m_window.handle()),
      m_flyController(m_input, m_window),
      m_swapChain(m_context),
      m_assetLibrary(m_context, options.project),
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
        MESH_BINDINGS,
        MESH_ATTRIBUTES
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
      ),

      m_reloadRequested(false) {
  m_viewSource = viewSource;

  m_storage.residency().attach(m_authoredWorld);

  m_skyPipeline.emplace(
    dunya::gpu::PipelineType::Sky,
    m_context.device().vkDevice(),
    dunya::renderer::pipelineSetLayouts(
      dunya::gpu::PipelineType::Sky,
      m_storage.frameGlobals(),
      m_storage.resourceTable(),
      m_storage.recordTable()
    ),
    m_swapChain
  );

  loadWorld(options);

  if (options.grid) {
    m_gridPipeline.emplace(
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
    );

    m_grid.emplace(m_context.device());
  }

  if (tools) {
    m_debugUi = tools(m_window, m_context, m_swapChain);
  }

  m_keySubscription = dunya::core::EventDispatcher::instance()
                        .subscribe<dunya::platform::KeyEvent>(
                          [this](const dunya::platform::KeyEvent& event) {
                            handleKeyEvent(event);
                          }
                        );

  m_mouseSubscription =
    dunya::core::EventDispatcher::instance()
      .subscribe<dunya::platform::MouseButtonEvent>(
        [this](const dunya::platform::MouseButtonEvent& event) {
          handleMouseButtonEvent(event);
        }
      );
}

Application::~Application() {
  try {
    stop();
  } catch (const std::exception& failure) {
    std::cerr << "Application teardown: " << failure.what() << std::endl;
  }

  dunya::core::EventDispatcher::instance()
    .unsubscribe<dunya::platform::MouseButtonEvent>(m_mouseSubscription);
  dunya::core::EventDispatcher::instance()
    .unsubscribe<dunya::platform::KeyEvent>(m_keySubscription);
}

void Application::loadWorld(const StartupOptions& options) {
  std::optional<dunya::serialize::Project> project =
    dunya::serialize::Project::open(options.project);

  if (!project.has_value()) {
    throw std::runtime_error("No project at " + options.project);
  }

  dunya::serialize::StoredWorld stored;

  if (!project->loadWorld(options.world, stored)) {
    throw std::runtime_error("No world named " + options.world);
  }

  if (!dunya::serialize::restoreWorld(
        stored,
        m_authoredWorld,
        m_assetLibrary.assets()
      )) {
    throw std::runtime_error("The world " + options.world + " did not load");
  }
}

void Application::drawSky(VkCommandBuffer commands) const {
  if (!m_skyPipeline) {
    return;
  }

  vkCmdBindPipeline(
    commands,
    VK_PIPELINE_BIND_POINT_GRAPHICS,
    m_skyPipeline->pipeline()
  );

  const VkDescriptorSet& globals =
    m_storage.frameGlobals().descriptorSet(m_renderer.currentFrame());

  vkCmdBindDescriptorSets(
    commands,
    VK_PIPELINE_BIND_POINT_GRAPHICS,
    m_skyPipeline->pipelineLayout(),
    0,
    1,
    &globals,
    0,
    nullptr
  );

  vkCmdDraw(commands, 3, 1, 0, 0);
}

void Application::drawGrid(VkCommandBuffer commands) const {
  if (!m_gridPipeline) {
    return;
  }

  vkCmdBindPipeline(
    commands,
    VK_PIPELINE_BIND_POINT_GRAPHICS,
    m_gridPipeline->pipeline()
  );

  const VkDescriptorSet& globals =
    m_storage.frameGlobals().descriptorSet(m_renderer.currentFrame());

  vkCmdBindDescriptorSets(
    commands,
    VK_PIPELINE_BIND_POINT_GRAPHICS,
    m_gridPipeline->pipelineLayout(),
    0,
    1,
    &globals,
    0,
    nullptr
  );

  m_grid->draw(commands, m_gridPipeline->pipelineLayout());
}

void Application::placeViewportCamera() {
  if (m_viewSource != ViewSource::ViewportCamera) {
    return;
  }

  const dunya::objectmodel::Entity eye =
    dunya::objectmodel::firstLens(m_authoredWorld);

  if (eye != dunya::objectmodel::INVALID_ENTITY) {
    const dunya::objectmodel::Pose& seat =
      m_authoredWorld.registry().get<dunya::objectmodel::Pose>(eye);

    const glm::vec3 ahead = seat.rotation * glm::vec3(0.0f, 0.0f, -1.0f);

    m_flyController.camera().place(
      seat.position,
      std::atan2(ahead.x, -ahead.z),
      std::asin(glm::clamp(ahead.y, -1.0f, 1.0f))
    );

    return;
  }

  const dunya::objectmodel::WorldExtent target =
    dunya::objectmodel::dynamicExtent(m_authoredWorld);

  if (target.empty) {
    return;
  }

  const dunya::objectmodel::Framing framing =
    dunya::objectmodel::frameExtent(target);

  m_flyController.camera().place(framing.position, framing.yaw, framing.pitch);
}

int Application::exportProject(const StartupOptions& options) {
  std::optional<dunya::serialize::Project> project =
    dunya::serialize::Project::open(options.exportProject);

  if (!project.has_value()) {
    project = dunya::serialize::Project::create(options.exportProject, "dunya");
  }

  if (!project.has_value()) {
    std::cerr << "Could not open or create a project at "
              << options.exportProject << '\n';

    return 1;
  }

  const dunya::serialize::StoredWorld stored =
    dunya::serialize::captureWorld(m_authoredWorld, m_assetLibrary.assets());

  if (!project->saveWorld(options.world, stored)) {
    std::cerr << "Could not write the world " << options.world << '\n';

    return 1;
  }

  std::cout << "Wrote " << stored.entities.size() << " entities to "
            << options.exportProject << '\n';

  return 0;
}

int Application::start(const StartupOptions& options) {
  if (!options.exportProject.empty()) {
    return exportProject(options);
  }

  glfwSetInputMode(m_window.handle(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);

  std::cout << "Click to fire at the wall, F to fire at its middle\n"
            << "Hold right mouse to look and fly (WASD/QE)\n"
            << "G resets the wall, F5 stops the simulation\n"
            << "Alt + click carves by hand once stopped\n";

  if (options.carves > 0) {
    carveForMeasurement(options.carves);
  }

  m_pendingDents = options.dents;
  m_dentLogPath = options.dentLog;

  m_deformScope =
    dunya::script::SdfDeformScope(&Application::onScriptDeform, this);

  if (!m_script.load("managed")) {
    throw std::runtime_error(
      "The script assembly did not load: " + m_script.lastError()
    );
  }

  if (!m_script.initialize(
        m_systems,
        m_authoredWorld,
        std::filesystem::path(options.project) / "scripts"
      )) {
    throw std::runtime_error(
      "The script assembly did not initialise: " + m_script.lastError()
    );
  }

  if (options.analytic) {
    m_frameContext.fieldRepresentation = dunya::core::FIELD_ANALYTIC;
    std::cout << "Field representation: analytic\n";
  }

  const uint32_t projectileMaterial =
    m_assetLibrary.assets().index<dunya::objectmodel::Material>(
      dunya::core::MATERIAL_PROJECTILE
    );

  m_canFire = projectileMaterial != dunya::core::UNBOUND_ASSET;

  if (!m_canFire) {
    std::cout << "This project has no projectile material; firing is off\n";
  }

  m_shotSettings = makeProjectile(
    m_canFire ? projectileMaterial : 0u,
    dunya::objectmodel::dynamicExtent(m_authoredWorld)
  );

  m_projectileField = bakeProjectile(m_shotSettings);

  placeViewportCamera();

  m_ballShape = new dunya::physics::FieldShape(m_projectileField);

  static_cast<void>(m_ballShape->GetMassProperties());

  m_ballVolume = m_storage.volumePool().allocate(m_projectileField);

  if (m_ballVolume != UINT32_MAX) {
    const auto ballImages = m_storage.volumePool().images(m_ballVolume);

    m_storage.recordTable().registerVolume(
      ballImages.distance.imageView(),
      ballImages.material.imageView(),
      m_ballVolume
    );

    m_storage.recordTable().uploadBounds(m_ballVolume, m_projectileField);
  }

  bool bakeCheckPending = options.verifyBake;

  FrameCheck frameCheck(m_context, m_swapChain, options);

  if (!frameCheck.wanted()) {
    announce("Baking fields", Transition::None);
  }

  std::function<void(VkImage)> captureHook;

  if (frameCheck.wanted() || frameCheck.capturing()) {
    captureHook = [&frameCheck](VkImage image) {
      frameCheck.run(image);
    };
  }

  if (frameCheck.capturing()) {
    std::filesystem::create_directories(options.capture);

    std::cout << "Recording every frame to " << options.capture << '\n';
  }

  double prevTime = glfwGetTime();
  double pipelineReloadCheck = 0;
  double statWindowStart = prevTime;
  double physicsAccumulator = 0.0;
  uint32_t statFrames = 0;

  double statCaptureSeconds = 0.0;

  while (!glfwWindowShouldClose(m_window.handle())) {
    double now = glfwGetTime();

    const float realDt = static_cast<float>(now - prevTime);

    float dt = realDt;

    prevTime = now;

    if (frameCheck.capturing()) {
      dt = 1.0f / 60.0f;
    }

    ++m_frameIndex;

    m_storage.uploader().retire();

    constexpr uint32_t FIRST_PLAY_FRAME = 4u;

    const bool measuring = frameCheck.wanted() || m_pendingDents > 0;

    const bool simulatesOnItsOwn = m_debugUi == nullptr;

    if (
      m_frameIndex == FIRST_PLAY_FRAME && simulatesOnItsOwn && !measuring
      && !m_runtime
    ) {
      play();
    }

    if (m_runtime) {
      physicsAccumulator += std::min(double(dt), MAX_PHYSICS_DELTA);

      uint32_t substeps = 0;

      const auto stepStart = std::chrono::steady_clock::now();

      while (physicsAccumulator >= dunya::physics::PhysicsWorld::TIME_STEP
             && substeps != MAX_PHYSICS_SUBSTEPS) {
        ++substeps;
        m_runtime->step();

        physicsAccumulator -= dunya::physics::PhysicsWorld::TIME_STEP;
      }

      m_telemetry.set(
        m_telemetry.key("physics"),
        std::chrono::duration<double, std::milli>(
          std::chrono::steady_clock::now() - stepStart
        )
          .count()
      );

      m_telemetry.set(m_telemetry.key("substeps"), double(substeps));

      m_telemetry.set(
        m_telemetry.key("awake"),
        double(m_runtime->physics().system().GetNumActiveBodies(
          JPH::EBodyType::RigidBody
        ))
      );

      {
        const auto carveStart = std::chrono::steady_clock::now();

        m_deformation.applyImpacts(*m_runtime, m_telemetry);

        m_telemetry.set(
          m_telemetry.key("carve"),
          std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - carveStart
          )
            .count()
        );
      }

      m_runtime->syncPoses(
        float(physicsAccumulator / dunya::physics::PhysicsWorld::TIME_STEP)
      );
    }

    ++statFrames;
    statCaptureSeconds += frameCheck.lastCaptureMs() / 1000.0;
    if (now - statWindowStart >= 1.0) {
      const double elapsed = now - statWindowStart - statCaptureSeconds;

      const double msPerFrame = (elapsed * 1000.0) / statFrames;
      m_lastFrameMs = msPerFrame;

      statCaptureSeconds = 0.0;

      std::cout << dunya::renderer::drawModeName(m_frameContext.mode) << "  "
                << m_swapChain.extent().width << "x"
                << m_swapChain.extent().height << "  " << std::fixed
                << std::setprecision(2) << msPerFrame << " ms  "
                << std::setprecision(0) << (statFrames / elapsed)
                << " fps  balls " << m_balls.size() << "\n"
                << std::defaultfloat << std::setprecision(6);

      statWindowStart = now;
      statFrames = 0;
    }

    glfwPollEvents();

    bool checkReady = now - pipelineReloadCheck > 0.5;

    if (checkReady) {
      pipelineReloadCheck = now;
    }

    if (m_reloadRequested || checkReady) {
      if (m_reloadRequested) {
        m_reloadRequested = false;
        m_context.device().waitIdle();
        m_meshPipeline.reload();
        m_sdfPipeline.reload();
      } else {
        if (m_meshPipeline.sourcesChanged()) {
          m_context.device().waitIdle();
          m_meshPipeline.reload();
        }
        if (m_sdfPipeline.sourcesChanged()) {
          m_context.device().waitIdle();
          m_sdfPipeline.reload();
        }
      }
    }

    if (m_viewSource == ViewSource::ViewportCamera) {
      m_flyController.update(dt, acceptsInput());
    }

    float aspect = static_cast<float>(m_swapChain.extent().width)
                   / static_cast<float>(m_swapChain.extent().height);

    lookThrough(aspect);

    const bool announcing = m_stall == Stall::Announced;

    if (announcing) {
      m_stall = Stall::Working;
    } else if (m_stall == Stall::Working) {
      if (m_transition == Transition::ToRuntime) {
        play();
      } else if (m_transition == Transition::ToAuthoring) {
        stop();
      } else if (m_transition == Transition::Restart) {
        stop();
        play();
      }

      m_transition = Transition::None;
      m_stall = Stall::None;
      if (m_debugUi) {
        m_debugUi->notice("");
      }
    }

    dunya::objectmodel::World& world = activeWorld();

    dunya::systems::Context systemContext{world, dt, m_frameIndex};
    m_systems.run(systemContext);

    refreshSystemsPanel();

    const entt::registry& registry = world.registry();

    m_frameContext.environment.reset();
    m_frameContext.light.reset();

    m_storage.framePacker().pack(
      m_frameContext,
      world,
      announcing ? std::span<const dunya::objectmodel::Entity>()
                 : world.sdfGrids(),
      m_assetLibrary.meshBuffers(),
      m_runtime ? std::function<void(dunya::objectmodel::Entity)>(
                    [this](dunya::objectmodel::Entity entity) {
                      m_runtime->refreshBody(entity);
                    }
                  )
                : std::function<void(dunya::objectmodel::Entity)>()
    );

    if (m_pendingDents > 0 && !announcing) {
      constexpr uint32_t DENTS_PER_FRAME = 1u;

      const uint32_t chunk = std::min(m_pendingDents, DENTS_PER_FRAME);

      dent(chunk);
      m_pendingDents -= chunk;

      if (m_pendingDents == 0 && !m_dentLogPath.empty()) {
        std::cout << "dents complete: " << m_dentsApplied << " logged to "
                  << m_dentLogPath << std::endl;

        glfwSetWindowShouldClose(m_window.handle(), GLFW_TRUE);
      }
    }

    {
      const auto uploadStart = std::chrono::steady_clock::now();

      uploadDentedVolumes();

      m_telemetry.set(
        m_telemetry.key("upload"),
        std::chrono::duration<double, std::milli>(
          std::chrono::steady_clock::now() - uploadStart
        )
          .count()
      );
    }

    std::vector<dunya::renderer::ScenePass> passes;

    if (m_frameContext.environment.has_value()) {
      passes.push_back(
        {dunya::renderer::PassOrder::BeforeScene,
         [this](VkCommandBuffer commandBuffer) { drawSky(commandBuffer); }}
      );
    }

    if (m_grid) {
      m_grid->update(glm::vec3(m_frameContext.cameraPos));

      passes.push_back(
        {dunya::renderer::PassOrder::AfterScene,
         [this](VkCommandBuffer commandBuffer) { drawGrid(commandBuffer); }}
      );
    }

    if (m_debugUi && !captureHook) {
      m_debugUi->build(m_panels);

      passes.push_back(
        {dunya::renderer::PassOrder::AfterScene,
         [this](VkCommandBuffer commandBuffer) {
           m_debugUi->record(commandBuffer);
         }}
      );
    }

    const bool swapChainStale =
      m_window.takeResized()
      || m_renderer.drawFrame(m_swapChain, m_frameContext, passes, captureHook);

    if (!swapChainStale) {
      m_storage.framePacker().commitBakes();
    } else {
      m_swapChain.recreate();
    }
    if (bakeCheckPending && !announcing) {
      m_context.device().waitIdle();

      uint32_t checked = 0;

      for (dunya::objectmodel::Entity entity : world.sdfGrids()) {
        const dunya::objectmodel::SdfGrid& grid =
          registry.get<dunya::objectmodel::SdfGrid>(entity);
        const auto* volume =
          registry.try_get<dunya::objectmodel::BakedVolume>(entity);

        if (volume == nullptr) {
          continue;
        }

        std::span<const dunya::field::Primitive> primitives =
          world.primitives(entity);
        dunya::renderer::VolumeImages images =
          m_storage.volumePool().images(volume->index);
        m_storage.sdfBaker().verifyBake(
          grid,
          volume->index,
          primitives,
          images
        );

        ++checked;
      }

      bakeCheckPending = checked == 0;
    }

    if (frameCheck.ran()) {
      glfwSetWindowShouldClose(m_window.handle(), GLFW_TRUE);
    }
  }

  m_context.device().waitIdle();

  if (bakeCheckPending) {
    std::cout << "bake check: nothing was verified" << std::endl;

    return 1;
  }

  return frameCheck.failed() ? 1 : 0;
}

bool Application::acceptsInput() const noexcept {
  return m_input.enabled() && m_window.focused();
}

void Application::handleMouseButtonEvent(
  const dunya::platform::MouseButtonEvent& event
) {
  if (!acceptsInput()) {
    return;
  }

  if (!m_flyController.looking() && m_debugUi && m_debugUi->wantsMouse()) {
    return;
  }

  if (event.button == GLFW_MOUSE_BUTTON_RIGHT) {
    m_flyController.setLookMode(
      event.type == dunya::platform::MouseButtonEventType::Pressed
    );
    return;
  }

  if (
    event.button != GLFW_MOUSE_BUTTON_LEFT
    || event.type != dunya::platform::MouseButtonEventType::Pressed
  ) {
    return;
  }

  if (m_flyController.looking()) {
    return;
  }

  if (m_runtime && (event.mods & GLFW_MOD_ALT) == 0) {
    fire(aimAtCursor());

    return;
  }
}

dunya::objectmodel::World& Application::activeWorld() noexcept {
  return m_runtime ? m_runtime->world() : m_authoredWorld;
}

const dunya::objectmodel::World& Application::activeWorld() const noexcept {
  return m_runtime ? m_runtime->world() : m_authoredWorld;
}

glm::vec3 Application::aimAtPoint(const glm::vec3& target) const {
  const glm::vec3 from = glm::vec3(m_frameContext.cameraPos);

  const glm::vec3 muzzle(from.x, m_shotSettings.height, from.z);
  const glm::vec3 toTarget = target - muzzle;

  if (glm::length(toTarget) < 1.0e-3f) {
    return glm::vec3(0.0f, 0.0f, -1.0f);
  }

  return glm::normalize(toTarget);
}

glm::vec3 Application::aimAtTarget() const {
  return aimAtPoint(m_shotSettings.aimAt);
}

glm::vec3 Application::aimAtCursor() const {
  const dunya::field::Ray ray = m_flyController.cursorRay(
    m_swapChain.extent(),
    m_frameContext.proj * m_frameContext.view
  );

  if (glm::length(ray.direction) < 1.0e-3f) {
    return aimAtTarget();
  }

  return glm::normalize(ray.direction);
}

void Application::fire(const glm::vec3& aim) {
  if (!m_runtime || !m_canFire) {
    return;
  }

  const Projectile& shot = m_shotSettings;
  const glm::vec3 from = glm::vec3(m_frameContext.cameraPos);

  const glm::vec3 muzzle(from.x, shot.height, from.z);

  if (m_balls.size() == MAX_BALLS) {
    m_runtime->despawn(m_balls.front());
    m_balls.pop_front();
  }

  dunya::objectmodel::World& world = m_runtime->world();

  dunya::objectmodel::Pose pose{};
  pose.position = muzzle + aim * MUZZLE_DISTANCE;

  const dunya::objectmodel::Entity ball = world.createSdfGrid(pose, shot.grid);

  if (!world.addPrimitive(ball, shot.shape)) {
    static_cast<void>(world.destroy(ball));
    std::cout << "Primitive arena full, no ball fired\n";

    return;
  }

  if (m_ballVolume != UINT32_MAX) {
    m_storage.volumePool().retain(m_ballVolume);
    world.setBakedVolume(ball, m_ballVolume);
    world.markBaked(ball);
  }

  m_balls.push_back(ball);

  m_runtime->setBodyShape(ball, m_ballShape);
  m_runtime->setMass(ball, shot.mass);
  m_runtime->launch(ball, aim * shot.speed);
}

void Application::announce(std::string text, Transition transition) {
  if (m_debugUi) {
    m_debugUi->notice(std::move(text));
  }
  m_transition = transition;
  m_stall = Stall::Announced;
}

void Application::onScriptDeform(
  void* host,
  uint32_t entity,
  const dunya::script::SdfDeformSummary* summary
) {
  auto* application = static_cast<Application*>(host);

  if (application == nullptr || summary == nullptr) {
    return;
  }

  const auto subject =
    static_cast<dunya::objectmodel::Entity>(entt::entity{entity});

  const glm::uvec3 begin(
    summary->brickBegin[0],
    summary->brickBegin[1],
    summary->brickBegin[2]
  );
  const glm::uvec3 end(
    summary->brickEnd[0],
    summary->brickEnd[1],
    summary->brickEnd[2]
  );

  if (application->m_runtime) {
    application->m_runtime->reshapeAfterDeform(subject, begin, end);
  }

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

  application->activeWorld().markSdfDirty(subject, touched);

  if (application->m_runtime) {
    dunya::objectmodel::World& world = application->m_runtime->world();

    if (world.registry().all_of<dunya::objectmodel::Pose>(subject)) {
      const glm::mat4 model = dunya::objectmodel::model(
        world.registry().get<dunya::objectmodel::Pose>(subject)
      );

      const glm::vec3 at = glm::vec3(model[3]);

      application->m_runtime->wake(at - glm::vec3(4.0f), at + glm::vec3(4.0f));
    }
  }
}

void Application::refreshSystemsPanel() {
  const std::span<const dunya::systems::Entry> entries = m_systems.systems();

  if (entries.size() == m_systemsShown) {
    return;
  }

  m_systemsShown = entries.size();

  dunya::debugui::Panel& panel = m_panels.panel("Systems");

  panel = dunya::debugui::Panel("Systems");

  panel.value("total", [this] { return m_systems.lastMilliseconds(); }, "ms");

  panel.separator();

  for (const dunya::systems::Entry& entry : entries) {
    const std::string label = std::to_string(entry.order) + " " + entry.name;

    const std::string name = entry.name;

    panel.value(
      label,
      [this, name] { return m_systems.lastMilliseconds(name); },
      "ms"
    );

    panel.toggle(
      label + " on",
      [this, name] { return m_systems.enabled(name) ? 1.0 : 0.0; },
      [this, name](double wanted) {
        static_cast<void>(m_systems.enable(name, wanted != 0.0));
      }
    );
  }
}

void Application::play() {
  if (m_runtime) {
    return;
  }

  m_storage.residency().releaseAll(activeWorld());

  m_runtime.emplace(m_authoredWorld, m_joltLibrary);

  m_storage.residency().attach(m_runtime->world());

  std::cout << "Play" << std::endl;
}

void Application::dent(uint32_t count) {
  dunya::objectmodel::World& world = activeWorld();
  const dunya::objectmodel::Entity target =
    dunya::objectmodel::firstDeformable(activeWorld());

  if (!world.registry().valid(target) || !world.hasSampledSdf(target)) {
    std::cout << "Nothing deformable to dent yet\n";
    return;
  }

  const std::optional<dunya::field::Aabb> extent =
    dunya::field::boundedExtent(world.primitives(target));

  if (!extent.has_value()) {
    std::cout << "The deformable has no bounded extent\n";
    return;
  }

  const glm::vec3 span = extent->maximum - extent->minimum;

  dunya::field::SampleBox touched{};

  std::ofstream log;

  if (!m_dentLogPath.empty()) {
    const bool fresh = m_dentsApplied == 0u;

    log.open(m_dentLogPath, fresh ? std::ios::trunc : std::ios::app);

    if (fresh) {
      log << "dent,deform_us,reshape_us,bricks,resident_bytes,primitives,"
             "sweep_settled\n";
    }
  }

  world.patchSampledSdf(target, [&](dunya::field::SampledSdf& field) {
    for (uint32_t i = 0; i < count; ++i) {
      const glm::vec2 at = glm::fract(
        static_cast<float>(m_dentsApplied + i)
        * glm::vec2(0.7548777f, 0.5698403f)
      );

      const glm::vec3 centre(
        extent->minimum.x + span.x * at.x,
        extent->maximum.y,
        extent->minimum.z + span.z * at.y
      );

      dunya::field::Primitive cutter = dunya::field::makeSphere(
        centre,
        dunya::core::EDIT_RADIUS,
        1u,
        dunya::core::FIELD_OP_SUBTRACTION
      );

      dunya::field::updateBounds(cutter);

      const auto deformStart = std::chrono::steady_clock::now();

      const dunya::field::DeformReport outcome =
        dunya::field::deformAndRepair(field, cutter);

      const dunya::field::WriteReport& report = outcome.write;

      const auto deformEnd = std::chrono::steady_clock::now();

      touched = dunya::field::merge(touched, report.samples);

      auto reshapeEnd = deformEnd;

      if (m_runtime) {
        m_runtime->reshapeAfterDeform(
          target,
          report.brickBegin,
          report.brickEnd
        );

        const glm::vec3 pad(dunya::core::EDIT_RADIUS);

        m_runtime->wake(centre - pad * 2.0f, centre + pad * 2.0f);

        reshapeEnd = std::chrono::steady_clock::now();
      }

      if (log.is_open()) {
        const glm::uvec3 moved = report.brickEnd - report.brickBegin;

        const glm::uvec3 counts = dunya::field::brickCounts(field);

        const size_t bricks =
          static_cast<size_t>(counts.x) * counts.y * counts.z;

        const size_t resident =
          field.distances.size() * sizeof(float)
          + field.materials.size() * sizeof(uint8_t)
          + field.brickLipschitz.size() * sizeof(float)
          + field.brickMinimum.size() * sizeof(float)
          + field.brickMaximum.size() * sizeof(float)
          + bricks
              * (sizeof(dunya::physics::SolidIntegral) + sizeof(dunya::physics::FieldSeed));

        log << (m_dentsApplied + i) << ','
            << std::chrono::duration_cast<std::chrono::microseconds>(
                 deformEnd - deformStart
               )
                 .count()
            << ','
            << std::chrono::duration_cast<std::chrono::microseconds>(
                 reshapeEnd - deformEnd
               )
                 .count()
            << ',' << (moved.x * moved.y * moved.z) << ',' << resident << ','
            << world.primitiveCount(target) << ',' << outcome.converged << '\n';
      }
    }
  });

  m_dentsApplied += count;

  activeWorld().markSdfDirty(target, touched);
}

void Application::uploadDentedVolumes() {
  m_storage.residency().flush(activeWorld(), m_telemetry);

  if (
    m_telemetry.get(m_telemetry.key("dentsDropped")) > 0.0
    && !m_splitFailureReported
  ) {
    m_splitFailureReported = true;

    std::cout << "Volume pool full, a dent is not drawn\n";
  }
}

void Application::stop() {
  if (!m_runtime) {
    return;
  }

  m_storage.residency().releaseAll(activeWorld());

  m_balls.clear();

  m_runtime.reset();

  std::cout << "Stop" << std::endl;
}

void Application::handleKeyEvent(const dunya::platform::KeyEvent& event) {
  if (m_debugUi && m_debugUi->wantsKeyboard() && event.key != GLFW_KEY_ESCAPE) {
    return;
  }

  if (
    event.key == GLFW_KEY_ESCAPE
    && event.type == dunya::platform::KeyEventType::Pressed
  ) {
    m_input.toggleEnabled();

    m_flyController.clear();

    m_flyController.setLookMode(false);
  }

  if (
    event.key == GLFW_KEY_P
    && event.type == dunya::platform::KeyEventType::SinglePressed
  ) {
    m_drawMode = dunya::renderer::nextDrawMode(m_drawMode);
    m_frameContext.mode = m_drawMode;
    std::cout << "Draw mode switched to: "
              << dunya::renderer::drawModeName(m_frameContext.mode) << '\n';
  }

  if (
    event.key == GLFW_KEY_B
    && event.type == dunya::platform::KeyEventType::SinglePressed
  ) {
    return;
  }

  if (
    event.key == GLFW_KEY_N
    && event.type == dunya::platform::KeyEventType::SinglePressed
  ) {
    dent(m_input.isDown(GLFW_KEY_LEFT_SHIFT) ? 100u : 1u);
    return;
  }

  if (
    event.key == GLFW_KEY_F5
    && event.type == dunya::platform::KeyEventType::SinglePressed
  ) {
    if (m_stall == Stall::None) {
      announce(
        m_runtime ? "Leaving play mode" : "Entering play mode",
        m_runtime ? Transition::ToAuthoring : Transition::ToRuntime
      );
    }

    return;
  }

  if (
    event.key == GLFW_KEY_G
    && event.type == dunya::platform::KeyEventType::SinglePressed
  ) {
    if (m_stall == Stall::None && m_runtime) {
      announce("Resetting", Transition::Restart);
    }

    return;
  }

  if (
    event.key == GLFW_KEY_F
    && event.type == dunya::platform::KeyEventType::SinglePressed
  ) {
    fire(aimAtCursor());

    return;
  }

  if (
    event.key == GLFW_KEY_R
    && event.type == dunya::platform::KeyEventType::SinglePressed
  ) {
    m_reloadRequested = true;
    return;
  }

  if (
    event.key == GLFW_KEY_V
    && event.type == dunya::platform::KeyEventType::SinglePressed
  ) {
    m_context.device().waitIdle();
    m_swapChain.setUncapped(!m_swapChain.uncapped());
    std::cout << "Present mode: "
              << (m_swapChain.uncapped() ? "immediate" : "fifo/mailbox")
              << '\n';
    return;
  }

  if (
    event.key == GLFW_KEY_Z
    && event.type == dunya::platform::KeyEventType::Pressed
  ) {
    if (m_input.isDown(GLFW_KEY_LEFT_CONTROL)) {
    }
  }

  if (
    event.key == GLFW_KEY_Y
    && event.type == dunya::platform::KeyEventType::Pressed
  ) {
    if (m_input.isDown(GLFW_KEY_LEFT_CONTROL)) {
    }
  }
  static_cast<void>(m_flyController.handleKey(event, acceptsInput()));
}

void Application::carveForMeasurement(uint32_t count) {
  if (m_authoredWorld.fields().empty()) {
    std::cout << "No field to carve into\n";
    return;
  }

  const dunya::objectmodel::Entity target = m_authoredWorld.fields()[0];

  const std::optional<dunya::field::Aabb> extent =
    dunya::field::boundedExtent(m_authoredWorld.primitives(target));

  if (!extent.has_value()) {
    std::cout << "Nothing bounded to carve into\n";
    return;
  }

  const glm::vec3 span = extent->maximum - extent->minimum;
  const uint32_t before = m_authoredWorld.primitiveCount(target);

  for (uint32_t i = 0; i != count; ++i) {
    const glm::vec3 at = glm::fract(
      static_cast<float>(before + i)
      * glm::vec3(0.8191725f, 0.6710436f, 0.5497005f)
    );

    dunya::field::Primitive cutter = dunya::field::makeSphere(
      extent->minimum + span * at,
      dunya::core::EDIT_RADIUS,
      0u,
      dunya::core::FIELD_OP_SUBTRACTION,
      0.0f
    );

    dunya::field::updateBounds(cutter);

    if (!m_authoredWorld.addPrimitive(target, cutter)) {
      std::cout << "Primitive budget full\n";
      break;
    }
  }

  std::cout << "stress  primitives " << before << " -> "
            << m_authoredWorld.primitiveCount(target) << "\n";
}

void Application::takeView(const dunya::objectmodel::CameraView& camera) {
  m_frameContext.proj = camera.projection;
  m_frameContext.view = camera.view;
  m_frameContext.cameraPos = glm::vec4(camera.position, camera.nearPlane);
}

void Application::lookThrough(float aspect) {
  if (m_viewSource == ViewSource::ViewportCamera) {
    m_frameContext.mode = m_drawMode;

    m_frameContext.proj = m_flyController.camera().projectionMatrix(aspect);
    m_frameContext.view = m_flyController.camera().viewMatrix();
    m_frameContext.cameraPos = m_flyController.camera().position();

    return;
  }

  const std::optional<dunya::objectmodel::CameraView> scene =
    dunya::objectmodel::activeCamera(activeWorld(), aspect);

  if (scene.has_value()) {
    takeView(*scene);

    m_frameContext.mode = m_drawMode;

    return;
  }

  if (!m_reportedMissingCamera) {
    m_reportedMissingCamera = true;

    std::cout << "no camera in this world, so it draws nothing\n";
  }

  m_frameContext.mode = dunya::renderer::DrawMode::Nothing;
}
