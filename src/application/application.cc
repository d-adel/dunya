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

const dunya::objectmodel::Pose& drawnPose(
  const entt::registry& registry,
  dunya::objectmodel::Entity entity
) {
  if (
    const auto* drawn = registry.try_get<dunya::objectmodel::RenderPose>(entity)
  ) {
    return drawn->pose;
  }

  return registry.get<dunya::objectmodel::Pose>(entity);
}

}

Application::Application(const StartupOptions& options, ToolsFactory tools)
    : m_input(m_context.window().handle()),
      m_cameraController(m_input, m_context.window()),
      m_swapChain(m_context),
      m_scene(
        m_context,
        m_authoredWorld,
        options.wallColumns,
        options.wallRows,
        options.wallDepth
      ),
      m_uploader(m_context.device()),
      m_frameGlobals(m_context.device()),
      m_resourceTable(
        m_context.device(),
        m_scene.textures(),
        m_scene.samplers(),
        m_scene.materials()
      ),
      m_recordTable(m_context.device()),
      m_fieldBaker(m_context.device(), m_recordTable),
      m_volumePool(m_context.device()),
      m_residency(m_volumePool, m_recordTable, m_uploader),
      m_meshPipeline(
        dunya::gpu::PipelineType::Mesh,
        m_context.device().vkDevice(),
        std::vector<VkDescriptorSetLayout>{

          m_frameGlobals.setLayout(),
          m_resourceTable.setLayout()
        },
        m_swapChain,
        MESH_BINDINGS,
        MESH_ATTRIBUTES
      ),
      m_fieldPipeline(
        dunya::gpu::PipelineType::Field,
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
        m_fieldBaker,
        m_volumePool,
        m_frameGlobals,
        m_meshPipeline,
        m_fieldPipeline,
        m_resourceTable,
        m_context.surface().handle(),
        m_swapChain.imageCount()
      ),

      m_reloadRequested(false) {
  if (tools) {
    m_tools = tools(m_context, m_swapChain, m_authoredWorld);
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
  dunya::core::EventDispatcher::instance()
    .unsubscribe<dunya::platform::MouseButtonEvent>(m_mouseSubscription);
  dunya::core::EventDispatcher::instance()
    .unsubscribe<dunya::platform::KeyEvent>(m_keySubscription);
}

int Application::start(const StartupOptions& options) {
  glfwSetInputMode(
    m_context.window().handle(),
    GLFW_CURSOR,
    GLFW_CURSOR_NORMAL
  );

  std::cout << "Click to fire at the wall, F to fire at its middle\n"
            << "Hold right mouse to look and fly (WASD/QE)\n"
            << "G resets the wall, F5 stops the simulation\n"
            << "Alt + click carves by hand once stopped\n";

  if (options.carves > 0) {
    if (m_tools) {
      m_tools->stress(options.carves);
    }
  }

  m_pendingDents = options.dents;
  m_dentLogPath = options.dentLog;

  m_demo = DemoDriver(options.demo, options.demoRate);

  if (options.analytic) {
    m_frameContext.fieldRepresentation = dunya::core::FIELD_ANALYTIC;
    std::cout << "Field representation: analytic\n";
  }

  m_scene.frame(m_cameraController.camera());

  m_shotSettings = m_scene.projectile();

  m_ballShape = new dunya::physics::FieldShape(m_scene.projectileField());

  static_cast<void>(m_ballShape->GetMassProperties());

  m_ballVolume = m_volumePool.allocate(m_scene.projectileField());

  if (m_ballVolume != UINT32_MAX) {
    const auto ballImages = m_volumePool.images(m_ballVolume);

    m_recordTable.registerVolume(
      ballImages.distance.imageView(),
      ballImages.material.imageView(),
      m_ballVolume
    );

    m_recordTable.uploadBounds(m_ballVolume, m_scene.projectileField());
  }

  bool bakeCheckPending = options.verifyBake;
  bool tableFullReported = false;
  bool volumePoolFullReported = false;

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

  while (!glfwWindowShouldClose(m_context.window().handle())) {
    double now = glfwGetTime();

    const float realDt = static_cast<float>(now - prevTime);

    float dt = realDt;

    prevTime = now;

    if (frameCheck.capturing()) {
      dt = 1.0f / 60.0f;
    }

    ++m_frameIndex;

    m_uploader.retire();

    constexpr uint32_t FIRST_PLAY_FRAME = 4u;

    const bool measuring = frameCheck.wanted() || m_pendingDents > 0;

    const bool simulatesOnItsOwn = m_tools == nullptr || m_demo.active();

    if (
      m_frameIndex == FIRST_PLAY_FRAME && simulatesOnItsOwn && !measuring
      && !m_runtime
    ) {
      play();
    }

    if (m_demo.active()) {
      if (m_demo.fires(m_frameIndex)) {
        const glm::vec2 at = m_demo.target();

        fire(aimAtPoint(m_scene.groundPoint(at.x, at.y)));
      }

      m_telemetry.set(m_telemetry.key("frame"), double(realDt) * 1000.0);

      m_demo.record(m_frameIndex, m_telemetry);

      m_telemetry.clear();

      if (m_demo.finished(m_frameIndex)) {
        recordSceneTelemetry();
        m_demo.report(m_telemetry);
        glfwSetWindowShouldClose(m_context.window().handle(), GLFW_TRUE);
      }
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

        if (m_demo.active()) {
          for (const auto& crater : m_deformation.cratersThisFrame()) {
            std::cout << "  crater on " << static_cast<uint32_t>(crater.entity)
                      << "  impulse " << crater.impulse << "  depth "
                      << crater.depth << "  radius " << crater.radius
                      << std::endl;
          }
        }

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

      if (m_demo.active()) {
        m_demo.measureMotion(activeWorld().registry(), m_telemetry);
      }
    }

    ++statFrames;
    statCaptureSeconds += frameCheck.lastCaptureMs() / 1000.0;
    if (now - statWindowStart >= 1.0) {
      const double elapsed = now - statWindowStart - statCaptureSeconds;

      const double msPerFrame = (elapsed * 1000.0) / statFrames;
      m_lastFrameMs = msPerFrame;

      statCaptureSeconds = 0.0;

      std::cout << modeName(m_frameContext.mode) << "  "
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
        m_fieldPipeline.reload();
      } else {
        if (m_meshPipeline.sourcesChanged()) {
          m_context.device().waitIdle();
          m_meshPipeline.reload();
        }
        if (m_fieldPipeline.sourcesChanged()) {
          m_context.device().waitIdle();
          m_fieldPipeline.reload();
        }
      }
    }

    m_cameraController.update(dt, acceptsInput());

    float aspect = static_cast<float>(m_swapChain.extent().width)
                   / static_cast<float>(m_swapChain.extent().height);

    m_recordTable.newFrame();
    m_frameContext.proj = m_cameraController.camera().projectionMatrix(aspect);
    m_frameContext.view = m_cameraController.camera().viewMatrix();
    m_frameContext.cameraPos = m_cameraController.camera().position();

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
      if (m_tools) {
        m_tools->notice("");
      }
    }

    dunya::objectmodel::World& world = activeWorld();
    const entt::registry& registry = world.registry();

    m_residency.reclaim(world);

    uint32_t recordIndex = 0;

    m_recordEntities.clear();

    const std::span<const dunya::objectmodel::Entity> visiting =
      announcing ? std::span<const dunya::objectmodel::Entity>()
                 : world.fields();

    for (const dunya::objectmodel::Entity entity : visiting) {
      if (recordIndex == dunya::core::MAX_FIELD_RECORDS) {
        if (!tableFullReported) {
          tableFullReported = true;
          std::cout << "Field record table full, the rest are not drawn\n";
        }

        break;
      }

      const dunya::objectmodel::SdfGrid& grid =
        registry.get<dunya::objectmodel::SdfGrid>(entity);

      if (!registry.all_of<dunya::objectmodel::BakedVolume>(entity)) {
        std::span<const dunya::field::Primitive> primitives =
          world.primitives(entity);

        const dunya::field::SampledField* carried = world.sampledField(entity);

        const bool reusable =
          carried != nullptr && !world.needsResample(entity);

        const dunya::renderer::VolumeKey key =
          registry.all_of<dunya::objectmodel::Deformed>(entity)
            ? dunya::renderer::VolumeKey{}
            : dunya::renderer::volumeKey(primitives, grid.resolution);

        uint32_t index = reusable ? UINT32_MAX : m_volumePool.acquire(key);

        const dunya::objectmodel::Entity donor =
          index == UINT32_MAX ? dunya::objectmodel::INVALID_ENTITY
                              : m_residency.fieldOnSlot(world, index);

        if (
          index != UINT32_MAX && donor == dunya::objectmodel::INVALID_ENTITY
        ) {
          m_volumePool.release(index);

          index = UINT32_MAX;
        }

        dunya::field::SampledField baked;

        if (!reusable && donor == dunya::objectmodel::INVALID_ENTITY) {
          const dunya::field::Aabb box =
            dunya::objectmodel::gridBox(primitives);

          baked = dunya::field::bake(
            primitives,
            box.minimum,
            box.maximum,
            grid.resolution
          );
        }

        if (index == UINT32_MAX) {
          index = m_volumePool.allocate(reusable ? *carried : baked, key);
        }

        if (index == UINT32_MAX) {
          if (!volumePoolFullReported) {
            volumePoolFullReported = true;
            std::cout << "Volume pool full, object not drawn\n";
          }

          continue;
        }

        auto images = m_volumePool.images(index);

        m_recordTable.registerVolume(
          images.distance.imageView(),
          images.material.imageView(),
          index
        );

        m_residency.hold(entity, index);

        world.setBakedVolume(entity, index);

        if (donor != dunya::objectmodel::INVALID_ENTITY) {
          world.shareSampledField(donor, entity);
        } else if (!reusable) {
          world.setSampledField(entity, std::move(baked));
        }

        if (registry.all_of<dunya::objectmodel::Deformable>(entity)) {
          m_recordTable.uploadBounds(index, *world.sampledField(entity));
        }

        if (m_runtime) {
          m_runtime->refreshBody(entity);
        }
      }

      const auto* range =
        registry.try_get<dunya::objectmodel::SdfPrimitiveRange>(entity);

      const uint32_t primitiveOffset = range == nullptr ? 0u : range->offset;

      const uint32_t primitiveCount = range == nullptr ? 0u : range->count;

      m_recordTable.setRecord(
        recordIndex,
        primitiveOffset,
        primitiveCount,
        drawnPose(registry, entity),
        grid,
        registry.get<dunya::objectmodel::BakedVolume>(entity),
        m_frameContext.fieldRepresentation
      );

      if (world.needsBake(entity)) {
        if (registry.all_of<dunya::objectmodel::Deformable>(entity)) {
          world.markBaked(entity);
        } else {
          m_recordTable.appendToBakeList(recordIndex);

          if (
            m_runtime && world.needsResample(entity)
            && registry.all_of<
               dunya::objectmodel::RigidBody,
               dunya::objectmodel::SharedField>(entity)
          ) {
            const dunya::field::Aabb refit =
              dunya::objectmodel::gridBox(world.primitives(entity));

            world.setSampledField(
              entity,
              dunya::field::bake(
                world.primitives(entity),
                refit.minimum,
                refit.maximum,
                grid.resolution
              )
            );

            m_runtime->refreshBody(entity);
          }
        }
      }

      m_recordEntities.push_back(entity);
      ++recordIndex;
    }

    if (m_pendingDents > 0 && !announcing) {
      constexpr uint32_t DENTS_PER_FRAME = 1u;

      const uint32_t chunk = std::min(m_pendingDents, DENTS_PER_FRAME);

      dent(chunk);
      m_pendingDents -= chunk;

      if (m_pendingDents == 0 && !m_dentLogPath.empty()) {
        std::cout << "dents complete: " << m_dentsApplied << " logged to "
                  << m_dentLogPath << std::endl;

        glfwSetWindowShouldClose(m_context.window().handle(), GLFW_TRUE);
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

    m_frameContext.fieldRecordCount = recordIndex;
    m_scene.augmentFrameContext(m_frameContext, world);

    std::function<void(VkCommandBuffer)> overlayHook;

    if (m_tools && !captureHook) {
      m_tools->build(*this);

      overlayHook = [this](VkCommandBuffer commandBuffer) {
        m_tools->record(commandBuffer);
      };
    }

    const bool swapChainStale = m_context.window().takeResized()
                                || m_renderer.drawFrame(
                                  m_swapChain,
                                  m_frameContext,
                                  overlayHook,
                                  captureHook
                                );

    if (!swapChainStale) {
      for (uint32_t idx : m_recordTable.bakeList()) {
        world.markBaked(m_recordEntities[idx]);
      }
    } else {
      m_swapChain.recreate();
    }
    if (bakeCheckPending && !announcing) {
      m_context.device().waitIdle();

      uint32_t checked = 0;

      for (dunya::objectmodel::Entity entity : world.fields()) {
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
          m_volumePool.images(volume->index);
        m_fieldBaker.verifyBake(grid, volume->index, primitives, images);

        ++checked;
      }

      bakeCheckPending = checked == 0;
    }

    if (frameCheck.ran()) {
      glfwSetWindowShouldClose(m_context.window().handle(), GLFW_TRUE);
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
  return m_input.enabled() && m_context.window().focused();
}

void Application::handleMouseButtonEvent(
  const dunya::platform::MouseButtonEvent& event
) {
  if (!acceptsInput()) {
    return;
  }

  if (!m_cameraController.looking() && m_tools && m_tools->wantsMouse()) {
    return;
  }

  if (event.button == GLFW_MOUSE_BUTTON_RIGHT) {
    m_cameraController.setLookMode(
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

  if (m_cameraController.looking()) {
    return;
  }

  if (m_runtime && (event.mods & GLFW_MOD_ALT) == 0) {
    fire(aimAtCursor());

    return;
  }

  if (m_tools == nullptr) {
    return;
  }

  m_tools->edit(
    (event.mods & GLFW_MOD_SHIFT) != 0
      ? dunya::core::FIELD_OP_SMOOTH_UNION
      : dunya::core::FIELD_OP_SMOOTH_SUBTRACTION,
    m_cameraController.cursorRay(
      m_swapChain.extent(),
      m_frameContext.proj * m_frameContext.view
    )
  );
}

dunya::objectmodel::World& Application::activeWorld() noexcept {
  return m_runtime ? m_runtime->world() : m_authoredWorld;
}

const dunya::objectmodel::World& Application::activeWorld() const noexcept {
  return m_runtime ? m_runtime->world() : m_authoredWorld;
}

glm::vec3 Application::aimAtPoint(const glm::vec3& target) const {
  const glm::vec3 from = glm::vec3(m_cameraController.camera().position());

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
  const dunya::field::Ray ray = m_cameraController.cursorRay(
    m_swapChain.extent(),
    m_frameContext.proj * m_frameContext.view
  );

  if (glm::length(ray.direction) < 1.0e-3f) {
    return aimAtTarget();
  }

  return glm::normalize(ray.direction);
}

void Application::fire(const glm::vec3& aim) {
  if (!m_runtime) {
    return;
  }

  const Scene::Projectile& shot = m_shotSettings;
  const glm::vec3 from = glm::vec3(m_cameraController.camera().position());

  const glm::vec3 muzzle(from.x, shot.height, from.z);

  if (m_balls.size() == MAX_BALLS) {
    m_runtime->despawn(m_balls.front());
    m_balls.pop_front();
  }

  dunya::objectmodel::World& world = m_runtime->world();

  dunya::objectmodel::Pose pose{};
  pose.position = muzzle + aim * MUZZLE_DISTANCE;

  const dunya::objectmodel::Entity ball = world.createField(pose, shot.grid);

  if (!world.addPrimitive(ball, shot.shape)) {
    static_cast<void>(world.destroyField(ball));
    std::cout << "Primitive arena full, no ball fired\n";

    return;
  }

  if (m_ballVolume != UINT32_MAX) {
    m_volumePool.retain(m_ballVolume);
    m_residency.hold(ball, m_ballVolume);

    world.setBakedVolume(ball, m_ballVolume);
    world.markBaked(ball);
  }

  m_balls.push_back(ball);

  m_runtime->setBodyShape(ball, m_ballShape);
  m_runtime->setMass(ball, shot.mass);
  m_runtime->launch(ball, aim * shot.speed);
}

void Application::announce(std::string text, Transition transition) {
  if (m_tools) {
    m_tools->notice(std::move(text));
  }
  m_transition = transition;
  m_stall = Stall::Announced;
}

void Application::play() {
  if (m_runtime) {
    return;
  }

  m_residency.releaseAll(activeWorld());

  m_runtime.emplace(m_authoredWorld, m_joltLibrary);

  if (m_tools) {
    m_tools->retarget(m_runtime->world());
  }

  std::cout << "Play" << std::endl;
}

void Application::dent(uint32_t count) {
  dunya::objectmodel::World& world = activeWorld();
  const dunya::objectmodel::Entity target = m_scene.deformable();

  if (!world.registry().valid(target) || !world.hasSampledField(target)) {
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

  world.patchSampledField(target, [&](dunya::field::SampledField& field) {
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

  m_deformation.markDirty(target, touched);
}

void Application::recordSceneTelemetry() {
  const dunya::objectmodel::World& world = activeWorld();

  std::unordered_set<const dunya::field::SampledField*> lattices;

  size_t residentBytes = 0;

  for (const dunya::objectmodel::Entity entity : world.fields()) {
    if (const auto* field = world.sampledField(entity)) {
      if (lattices.insert(field).second) {
        residentBytes += field->distances.size() * sizeof(float)
                         + field->materials.size() * sizeof(uint8_t);
      }
    }
  }

  m_telemetry.set(m_telemetry.key("volumes"), double(m_volumePool.allocated()));
  m_telemetry.set(
    m_telemetry.key("volumeCapacity"),
    double(dunya::core::MAX_FIELD_VOLUMES)
  );
  m_telemetry.set(m_telemetry.key("lattices"), double(lattices.size()));
  m_telemetry.set(m_telemetry.key("objects"), double(world.fields().size()));
  m_telemetry.set(
    m_telemetry.key("residentMB"),
    double(residentBytes / (1024 * 1024))
  );
  m_telemetry.set(
    m_telemetry.key("shapes"),
    double(m_runtime ? m_runtime->shapeCount() : 0u)
  );
}

void Application::uploadDentedVolumes() {
  m_residency.upload(activeWorld(), m_deformation.dirty(), m_telemetry);

  m_deformation.clearDirty();

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

  m_residency.releaseAll(activeWorld());

  m_balls.clear();

  m_runtime.reset();

  if (m_tools) {
    m_tools->retarget(m_authoredWorld);
  }

  std::cout << "Stop" << std::endl;
}

Application::PanelSources Application::panelSources() {
  PanelSources frame{};

  frame.world = &activeWorld();
  frame.deformation = &m_deformation;
  frame.impacts = m_runtime ? &m_runtime->physics().impacts() : nullptr;
  frame.shot = &m_shotSettings;
  frame.march = &m_frameContext.march;

  frame.balls = m_balls.size();
  frame.maxBalls = MAX_BALLS;
  frame.primitives = activeWorld().pool().size();

  frame.playing = m_runtime.has_value();
  frame.analytic =
    m_frameContext.fieldRepresentation == dunya::core::FIELD_ANALYTIC;

  frame.frameMs = m_lastFrameMs;
  frame.extent = m_swapChain.extent();

  frame.fire = [this] {
    fire(aimAtTarget());
  };
  frame.resetWall = [this] {
    if (m_stall == Stall::None && m_runtime) {
      announce("Resetting", Transition::Restart);
    }
  };

  return frame;
}

void Application::handleKeyEvent(const dunya::platform::KeyEvent& event) {
  if (m_tools && m_tools->wantsKeyboard() && event.key != GLFW_KEY_ESCAPE) {
    return;
  }

  if (
    event.key == GLFW_KEY_ESCAPE
    && event.type == dunya::platform::KeyEventType::Pressed
  ) {
    m_input.toggleEnabled();

    m_cameraController.clear();

    m_cameraController.setLookMode(false);
  }

  if (
    event.key == GLFW_KEY_P
    && event.type == dunya::platform::KeyEventType::SinglePressed
  ) {
    m_frameContext.mode = nextPipelineType(m_frameContext.mode);
    std::cout << "Pipeline mode switched to: " << (int)m_frameContext.mode
              << '\n';
  }

  if (
    event.key == GLFW_KEY_B
    && event.type == dunya::platform::KeyEventType::SinglePressed
  ) {
    if (m_tools) {
      m_tools->stress(10);
    }
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
      if (m_tools) {
        m_tools->undo();
      }
    }
  }

  if (
    event.key == GLFW_KEY_Y
    && event.type == dunya::platform::KeyEventType::Pressed
  ) {
    if (m_input.isDown(GLFW_KEY_LEFT_CONTROL)) {
      if (m_tools) {
        m_tools->redo();
      }
    }
  }
  static_cast<void>(m_cameraController.handleKey(event, acceptsInput()));
}
