#include "application.ih"

namespace {

// Five is three frames of headroom at 60 Hz and still bounds the worst case.
constexpr uint32_t MAX_PHYSICS_SUBSTEPS = 5;

const std::vector<VkVertexInputBindingDescription> MESH_BINDINGS{
  dunya::renderer::Vertex::getBindingDescription()
};

const auto MESH_ATTRIBUTES =
  dunya::renderer::Vertex::getAttributeDescriptions();

}  // namespace

Application::Application()
    : m_input(m_context.window().handle()),
      m_swapChain(m_context),
      m_scene(m_context, m_authoredWorld),
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
      m_overlay(m_context, m_swapChain),
      m_fieldEditor(m_authoredWorld),
      m_cameraInput({}),
      m_prevAcceptsInput(false),
      m_reloadRequested(false)

{
  m_volumeOwners.fill(dunya::objectmodel::INVALID_ENTITY);

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

  std::cout << "Hold right mouse to look and fly (WASD/QE)\n"
            << "Left click carves, shift + left click adds\n";

  if (options.carves > 0) {
    m_fieldEditor.stress(options.carves);
  }

  if (options.analytic) {
    m_frameContext.fieldRepresentation = dunya::core::FIELD_ANALYTIC;
    std::cout << "Field representation: analytic\n";
  }

  registerPanels();

  bool bakeCheckPending = options.verifyBake;
  bool tableFullReported = false;
  bool volumePoolFullReported = false;

  FrameCheck frameCheck(m_context, m_swapChain, options);

  // Not in a capture run: the deferral would make the notice the first frame
  // presented, and that is the frame a golden compares.
  if (!frameCheck.wanted()) {
    announce("Baking... 🍞", Transition::None);
  }

  std::function<void(VkImage)> captureHook;

  if (frameCheck.wanted()) {
    captureHook = [&frameCheck](VkImage image) {
      frameCheck.run(image);
    };
  }

  double prevTime = glfwGetTime();
  double pipelineReloadCheck = 0;
  double statWindowStart = prevTime;
  double physicsAccumulator = 0.0;
  uint32_t statFrames = 0;

  while (!glfwWindowShouldClose(m_context.window().handle())) {
    double now = glfwGetTime();
    float dt = static_cast<float>(now - prevTime);

    prevTime = now;

    // E5: no runtime, no physics. The accumulator only advances while a
    // runtime exists, so Play does not begin by discharging a backlog.
    if (m_runtime) {
      physicsAccumulator += dt;

      // A slow frame must not buy more steps than the next frame can afford,
      // or the loop feeds itself: more steps, slower frame, more steps.
      uint32_t substeps = 0;

      while (physicsAccumulator >= dunya::physics::PhysicsWorld::TIME_STEP
             && substeps != MAX_PHYSICS_SUBSTEPS) {
        ++substeps;
        m_runtime->step();

        physicsAccumulator -= dunya::physics::PhysicsWorld::TIME_STEP;
      }

      // Whatever is left would be paid for next frame and start the spiral
      // again, so it is dropped: simulation time lags, frame rate does not.
      if (substeps == MAX_PHYSICS_SUBSTEPS) {
        physicsAccumulator = 0.0;
      }

      // Once per frame, not once per step: the world only needs where things
      // ended up, and the renderer reads it straight after.
      m_runtime->syncPoses();
    }

    ++statFrames;
    if (now - statWindowStart >= 1.0) {
      const double elapsed = now - statWindowStart;
      const double msPerFrame = (elapsed * 1000.0) / statFrames;
      m_lastFrameMs = msPerFrame;

      std::cout << modeName(m_frameContext.mode) << "  "
                << m_swapChain.extent().width << "x"
                << m_swapChain.extent().height << "  " << std::fixed
                << std::setprecision(2) << msPerFrame << " ms  "
                << std::setprecision(0) << (statFrames / elapsed)
                << " fps\n"
                // Both stick, so every float printed after the first second
                // would otherwise come out rounded to a whole number.
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

    bool curAcceptsInput = acceptsInput() && m_looking;

    if (curAcceptsInput != m_prevAcceptsInput) {
      m_input.cursorDeltaInvalid();
      m_prevAcceptsInput = curAcceptsInput;
    }

    m_input.update();

    if (curAcceptsInput) {
      m_cameraInput.lookDx = static_cast<float>(m_input.cursor().dx);
      m_cameraInput.lookDy = static_cast<float>(m_input.cursor().dy);
      m_camera.update(dt, m_cameraInput);
    }

    float aspect = static_cast<float>(m_swapChain.extent().width)
                   / static_cast<float>(m_swapChain.extent().height);

    m_recordTable.newFrame();
    // ---------- Frame context ----------
    m_frameContext.proj = m_camera.projectionMatrix(aspect);
    m_frameContext.view = m_camera.viewMatrix();
    m_frameContext.cameraPos = m_camera.position();

    // One frame of notice before a stall, and the work on the frame after: the
    // image left on screen while the loop blocks is then the message rather
    // than the picture from before the key was pressed.
    const bool announcing = m_stall == Stall::Announced;

    if (announcing) {
      m_stall = Stall::Working;
    } else if (m_stall == Stall::Working) {
      if (m_transition == Transition::ToRuntime) {
        play();
      } else if (m_transition == Transition::ToAuthoring) {
        stop();
      }

      m_transition = Transition::None;
      m_stall = Stall::None;
      m_overlay.notice("");
    }

    dunya::objectmodel::World& world = activeWorld();
    const entt::registry& registry = world.registry();

    // Reclaim slots whose entity is gone, before anything asks for a new one.
    // Done here rather than by a listener inside VolumePool, because the pool
    // is a renderer resource and the registry is world state: the frame is
    // where the two meet.
    for (uint32_t slot = 0; slot != m_volumeOwners.size(); ++slot) {
      const dunya::objectmodel::Entity owner = m_volumeOwners[slot];

      if (owner == dunya::objectmodel::INVALID_ENTITY) {
        continue;
      }

      if (
        registry.valid(owner)
        && registry.all_of<dunya::objectmodel::BakedVolume>(owner)
      ) {
        continue;
      }

      m_volumePool.release(slot);
      m_volumeOwners[slot] = dunya::objectmodel::INVALID_ENTITY;
    }

    uint32_t recordIndex = 0;

    m_recordEntities.clear();

    // The notice frame presents before the work rather than with it, so it
    // visits nothing and the records it leaves are last frame's.
    const std::span<const dunya::objectmodel::Entity> visiting =
      announcing ? std::span<const dunya::objectmodel::Entity>()
                 : world.fields();

    for (const dunya::objectmodel::Entity entity : visiting) {
      // The GPU record table holds MAX_FIELD_RECORDS.
      // The world has no such limit, so the frame is where the two meet.
      if (recordIndex == dunya::core::MAX_FIELD_RECORDS) {
        if (!tableFullReported) {
          tableFullReported = true;
          std::cout << "Field record table full, the rest are not drawn\n";
        }

        break;
      }

      const dunya::objectmodel::SdfGrid& grid =
        registry.get<dunya::objectmodel::SdfGrid>(entity);

      // Set by the branch below, read by the rebake after it: on the frame an
      // object first appears both fire, and the second bake would be of the
      // primitives the first one just read.
      bool cpuFieldFresh = false;
      // No BakedVolume means no pool slot yet. Absence is the state; there is
      // no sentinel to get wrong.

      if (!registry.all_of<dunya::objectmodel::BakedVolume>(entity)) {
        std::span<const dunya::field::Primitive> primitives =
          world.primitives(entity);

        const auto* carried =
          registry.try_get<dunya::field::SampledField>(entity);

        // A bake is seconds and a copy is milliseconds. The field the entity
        // already carries was made from these same primitives, so a volume can
        // be filled from it rather than from a second identical bake.
        const bool reusable =
          carried != nullptr && !world.needsResample(entity);

        dunya::field::SampledField baked;

        if (!reusable) {
          const dunya::field::Aabb box =
            dunya::objectmodel::gridBox(primitives);

          baked = dunya::field::bake(
            primitives,
            box.minimum,
            box.maximum,
            grid.resolution
          );
        }

        const uint32_t index =
          m_volumePool.allocate(reusable ? *carried : baked);

        // No volume means nothing to sample, and UINT32_MAX would index
        // the volume array out of bounds on the GPU. Skip it entirely.
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

        m_volumeOwners[index] = entity;

        world.setBakedVolume(entity, index);

        // Physics queries this, so a fresh bake is kept rather than dropped
        // once the volume is uploaded. A reused one is already in place.
        if (!reusable) {
          world.setSampledField(entity, std::move(baked));
        }

        cpuFieldFresh = true;

        // The body reads the field, so it cannot exist before one does: this
        // is where a runtime entity gets its body, a frame after Play.
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
        registry.get<dunya::objectmodel::Pose>(entity),
        grid,
        registry.get<dunya::objectmodel::BakedVolume>(entity),
        m_frameContext.fieldRepresentation
      );

      if (world.needsBake(entity)) {
        m_recordTable.appendToBakeList(recordIndex);

        // Physics reads the CPU field, so it has to follow the geometry. Only
        // where a body exists: nothing queries it while authoring, and a full
        // rebake is far too dear to run for a reader that is not there.
        if (
          m_runtime && !cpuFieldFresh
          && registry.all_of<dunya::objectmodel::RigidBody>(entity)
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

          // The old shape reads the field that call just replaced.
          m_runtime->refreshBody(entity);
        }
      }

      m_recordEntities.push_back(entity);

      ++recordIndex;
    }

    m_frameContext.fieldRecordCount = recordIndex;

    m_scene.augmentFrameContext(m_frameContext, world);
    // -----------------------------------

    // A capture run builds no overlay at all, which is what keeps it out of the
    // golden images without the renderer needing to know it exists.
    std::function<void(VkCommandBuffer)> overlayHook;

    if (!captureHook) {
      m_overlay.begin();
      m_overlay.build();
      m_overlay.end();

      overlayHook = [this](VkCommandBuffer commandBuffer) {
        m_overlay.record(commandBuffer);
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
      // Bake-list entries are packing slots, so they index the mapping the
      // packing loop recorded, not fields(). A skip consumes no slot.
      for (uint32_t idx : m_recordTable.bakeList()) {
        world.markBaked(m_recordEntities[idx]);
      }
    } else {
      m_swapChain.recreate();
    }
    // After a frame, because the compute bake runs as part of one. Before it,
    // the volumes still hold the CPU bake and the check would be comparing
    // that against itself.
    if (bakeCheckPending) {
      bakeCheckPending = false;
      m_context.device().waitIdle();
      for (dunya::objectmodel::Entity entity : world.fields()) {
        const dunya::objectmodel::SdfGrid& grid =
          registry.get<dunya::objectmodel::SdfGrid>(entity);
        const auto* volume =
          registry.try_get<dunya::objectmodel::BakedVolume>(entity);

        // Nothing was baked for this entity, so there is nothing to check
        // against. Reading the pool at a sentinel would be the bug the
        // component's absence exists to prevent.
        if (volume == nullptr) {
          continue;
        }

        std::span<const dunya::field::Primitive> primitives =
          world.primitives(entity);
        dunya::renderer::VolumeImages images =
          m_volumePool.images(volume->index);
        m_fieldBaker.verifyBake(grid, volume->index, primitives, images);
      }
    }

    if (frameCheck.ran()) {
      glfwSetWindowShouldClose(m_context.window().handle(), GLFW_TRUE);
    }
  }

  m_context.device().waitIdle();

  return frameCheck.failed() ? 1 : 0;
}

void Application::clearCameraInput() noexcept {
  m_cameraInput = {};
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

  // The overlay gets first refusal on the cursor, or a click on a slider also
  // carves behind it. Not while looking: the cursor is captured and its
  // reported position is virtual.
  if (!m_looking && m_overlay.wantsMouse()) {
    return;
  }

  if (event.button == GLFW_MOUSE_BUTTON_RIGHT) {
    setLookMode(event.type == dunya::platform::MouseButtonEventType::Pressed);
    return;
  }

  if (
    event.button != GLFW_MOUSE_BUTTON_LEFT
    || event.type != dunya::platform::MouseButtonEventType::Pressed
  ) {
    return;
  }

  // Clicking is for the visible cursor; while looking, the reported position
  // is a virtual one that has nothing to do with the screen.
  if (m_looking) {
    return;
  }

  // Both smooth: a stamp meets the one before it at a crease, and that is what
  // shading shows, whichever direction the material moved.
  m_fieldEditor.edit(
    (event.mods & GLFW_MOD_SHIFT) != 0
      ? dunya::core::FIELD_OP_SMOOTH_UNION
      : dunya::core::FIELD_OP_SMOOTH_SUBTRACTION,
    cursorRay()
  );
}

dunya::objectmodel::World& Application::activeWorld() noexcept {
  return m_runtime ? m_runtime->world() : m_authoredWorld;
}

void Application::releaseAllVolumes() {
  dunya::objectmodel::World& world = activeWorld();

  for (uint32_t slot = 0; slot != m_volumeOwners.size(); ++slot) {
    if (m_volumeOwners[slot] == dunya::objectmodel::INVALID_ENTITY) {
      continue;
    }

    // The component must go with the slot, or the world this belonged to
    // renders through a freed volume the next time it is active.
    if (world.registry().valid(m_volumeOwners[slot])) {
      world.clearBakedVolume(m_volumeOwners[slot]);
    }

    m_volumePool.release(slot);
    m_volumeOwners[slot] = dunya::objectmodel::INVALID_ENTITY;
  }
}

void Application::announce(std::string text, Transition transition) {
  m_overlay.notice(std::move(text));
  m_transition = transition;
  m_stall = Stall::Announced;
}

void Application::play() {
  if (m_runtime) {
    return;
  }

  // Identity is preserved across instantiation, so an authored entity id is
  // also valid in the runtime world. The owner table cannot tell them apart,
  // so every slot goes back and the new world bakes its own.
  releaseAllVolumes();

  m_runtime.emplace(m_authoredWorld, m_joltLibrary);

  // An edit has to land in the world that is on screen, and the runtime one
  // is from here on.
  m_fieldEditor.retarget(m_runtime->world());

  std::cout << "Play" << std::endl;
}

void Application::stop() {
  if (!m_runtime) {
    return;
  }

  releaseAllVolumes();

  m_runtime.reset();

  m_fieldEditor.retarget(m_authoredWorld);

  std::cout << "Stop" << std::endl;
}

void Application::setLookMode(bool looking) {
  if (m_looking == looking) {
    return;
  }

  m_looking = looking;

  if (!m_looking) {
    clearCameraInput();
  }

  // Entering or leaving capture teleports the cursor, and a delta across that
  // jump would spin the camera (idiom 17).
  m_input.cursorDeltaInvalid();

  glfwSetInputMode(
    m_context.window().handle(),
    GLFW_CURSOR,
    m_looking ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL
  );
}

// Every panel this process shows, declared where the data it reads lives. They
// capture this, which outlives them.
void Application::registerPanels() {
  m_overlay.panel("Frame", [this] {
    ImGui::Text(
      "%.2f ms  %.0f fps",
      m_lastFrameMs,
      m_lastFrameMs > 0.0 ? 1000.0 / m_lastFrameMs : 0.0
    );
    ImGui::Text(
      "%ux%u",
      m_swapChain.extent().width,
      m_swapChain.extent().height
    );

    size_t primitiveCount = m_authoredWorld.pool().size();

    ImGui::Text("%zu primitives", primitiveCount);
    ImGui::Text(
      "%s",
      m_frameContext.fieldRepresentation == dunya::core::FIELD_ANALYTIC
        ? "analytic"
        : "sampled"
    );
  });

  m_overlay.panel("March", [this] {
    dunya::renderer::MarchParams& march = m_frameContext.march;

    // Logarithmic where the useful range spans orders of magnitude: a linear
    // slider from 0.0001 to 0.01 spends nearly all of its travel in values
    // that make the march crawl.
    ImGui::SliderFloat(
      "epsilon",
      &march.epsilon,
      0.0001f,
      0.01f,
      "%.5f",
      ImGuiSliderFlags_Logarithmic
    );
    ImGui::SliderFloat(
      "gradient",
      &march.gradientEpsilon,
      0.001f,
      0.1f,
      "%.4f"
    );

    // Below 1 is plain sphere tracing and above 2 is unstable even when the
    // estimator is conservative.
    ImGui::SliderFloat("omega", &march.omega, 1.0f, 2.0f);

    // Above 1 would trust a value trilinear interpolation is known to
    // overestimate, which is how a march steps through a surface.

    ImGui::SliderFloat("max distance", &march.maxDistance, 10.0f, 500.0f);
    ImGui::SliderFloat(
      "shadow distance",
      &march.shadowMaxDistance,
      1.0f,
      100.0f
    );
    ImGui::SliderFloat("shadow sharpness", &march.shadowSharpness, 1.0f, 64.0f);

    int iterations = static_cast<int>(march.maxIterations);
    if (ImGui::SliderInt("max iterations", &iterations, 32, 2000)) {
      march.maxIterations = static_cast<uint32_t>(iterations);
    }
  });
}

dunya::field::Ray Application::cursorRay() const {
  const dunya::platform::Cursor cursor = m_input.cursor();
  const VkExtent2D extent = m_swapChain.extent();

  // Vulkan's NDC y runs downward because the projection flips it, and GLFW
  // reports the cursor from the top left, so both axes map without a flip.
  const glm::vec2 ndc(
    2.0f * static_cast<float>(cursor.x) / static_cast<float>(extent.width)
      - 1.0f,
    2.0f * static_cast<float>(cursor.y) / static_cast<float>(extent.height)
      - 1.0f
  );

  const glm::mat4 viewProj = m_frameContext.proj * m_frameContext.view;

  return dunya::field::screenPointToRay(
    glm::inverse(viewProj),
    glm::vec3(m_frameContext.cameraPos),
    ndc
  );
}

void Application::handleKeyEvent(const dunya::platform::KeyEvent& event) {
  // Typing into a text field must not also fly the camera. Escape is exempt
  // because it is how the cursor is handed back, and a UI that could swallow it
  // would be a UI you cannot leave.
  if (m_overlay.wantsKeyboard() && event.key != GLFW_KEY_ESCAPE) {
    return;
  }

  if (
    event.key == GLFW_KEY_ESCAPE
    && event.type == dunya::platform::KeyEventType::Pressed
  ) {
    m_input.toggleEnabled();

    clearCameraInput();

    // The cursor now belongs to look mode, so escape leaves it rather than
    // setting the mode itself.
    setLookMode(false);
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
    m_fieldEditor.stress(10);
    return;
  }

  if (
    event.key == GLFW_KEY_F5
    && event.type == dunya::platform::KeyEventType::SinglePressed
  ) {
    // Announced rather than done here: both directions stall the loop, and a
    // frame has to be presented before the stall for the message to be seen.
    if (m_stall == Stall::None) {
      announce(
        m_runtime ? "Leaving play mode" : "Entering play mode",
        m_runtime ? Transition::ToAuthoring : Transition::ToRuntime
      );
    }

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
      m_fieldEditor.undo();
    }
  }

  if (
    event.key == GLFW_KEY_Y
    && event.type == dunya::platform::KeyEventType::Pressed
  ) {
    if (m_input.isDown(GLFW_KEY_LEFT_CONTROL)) {
      m_fieldEditor.redo();
    }
  }

  bool* state = nullptr;

  switch (event.key) {
    case GLFW_KEY_W:
      state = &m_cameraInput.forward;
      break;

    case GLFW_KEY_S:
      state = &m_cameraInput.back;
      break;

    case GLFW_KEY_A:
      state = &m_cameraInput.left;
      break;

    case GLFW_KEY_D:
      state = &m_cameraInput.right;
      break;

    case GLFW_KEY_E:
      state = &m_cameraInput.up;
      break;

    case GLFW_KEY_Q:
      state = &m_cameraInput.down;
      break;
    default:
      return;
  }

  switch (event.type) {
    case dunya::platform::KeyEventType::Pressed:
      // Movement belongs to look mode, the same way it does in a scene view.
      *state = acceptsInput() && m_looking;
      break;

    case dunya::platform::KeyEventType::Released:
      *state = false;
      break;

    default:
      break;
  }
}
