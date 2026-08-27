#include "application.ih"

namespace {

const std::vector<VkVertexInputBindingDescription> MESH_BINDINGS{
  dunya::renderer::Vertex::getBindingDescription()
};

const auto MESH_ATTRIBUTES =
  dunya::renderer::Vertex::getAttributeDescriptions();

}  // namespace

Application::Application()
    : m_physicsDemo(m_physicsWorld),
      m_input(m_context.window().handle()),
      m_swapChain(m_context),
      m_scene(m_context),
      m_frameGlobals(m_context.device()),
      m_resourceTable(
        m_context.device(),
        m_scene.textures(),
        m_scene.samplers(),
        m_scene.materials()
      ),
      m_fieldObjectTable(m_context.device()),
      m_fieldBaker(m_context.device(), m_fieldObjectTable),
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
          m_fieldObjectTable.setLayout()
        },
        m_swapChain
      ),
      m_renderer(
        m_context.device(),
        m_fieldObjectTable,
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
      m_fieldEditor(m_scene.world()),
      m_cameraInput({}),
      m_prevAcceptsInput(false),
      m_reloadRequested(false)

{
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
  uint64_t physicsStep = 0;
  uint32_t statFrames = 0;

  while (!glfwWindowShouldClose(m_context.window().handle())) {
    double now = glfwGetTime();
    float dt = static_cast<float>(now - prevTime);

    prevTime = now;

    physicsAccumulator += dt;

    while (physicsAccumulator >= dunya::physics::PhysicsWorld::TIME_STEP) {
      m_physicsWorld.step();
      ++physicsStep;

      if (physicsStep % 30 == 0) {
        m_physicsDemo.log();
      }

      physicsAccumulator -= dunya::physics::PhysicsWorld::TIME_STEP;
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
                << std::setprecision(0) << (statFrames / elapsed) << " fps\n";

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

    m_fieldObjectTable.newFrame();
    // ---------- Frame context ----------
    m_frameContext.proj = m_camera.projectionMatrix(aspect);
    m_frameContext.view = m_camera.viewMatrix();
    m_frameContext.cameraPos = m_camera.position();

    dunya::objectmodel::World& world = m_scene.world();
    const entt::registry& registry = world.registry();

    uint32_t objectIndex = 0;

    for (dunya::objectmodel::Entity entity : world.fieldObjects()) {
      // The GPU table and its descriptor arrays hold MAX_FIELD_OBJECTS.
      // The world has no such limit, so the frame is where the two meet.
      if (objectIndex == dunya::core::MAX_FIELD_OBJECTS) {
        if (!tableFullReported) {
          tableFullReported = true;
          std::cout << "Field object table full, the rest are not drawn\n";
        }

        break;
      }

      const dunya::objectmodel::FieldObject& fieldObject =
        registry.get<dunya::objectmodel::FieldObject>(entity);

      if (fieldObject.volumeIndex == UINT32_MAX) {
        std::span<const dunya::field::Primitive> primitives =
          world.primitives(entity);

        dunya::field::Aabb box = dunya::objectmodel::gridBox(primitives);
        const dunya::field::SampledField& grid = dunya::field::bake(
          primitives,
          box.minimum,
          box.maximum,
          fieldObject.resolution
        );

        const uint32_t index = m_volumePool.allocate(grid);

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

        m_fieldObjectTable.registerVolume(
          images.distance.imageView(),
          images.material.imageView(),
          index
        );

        world.setVolumeIndex(entity, index);
      }

      const auto* range =
        registry.try_get<dunya::objectmodel::SdfPrimitiveRange>(entity);

      const uint32_t primitiveOffset = range == nullptr ? 0u : range->offset;

      const uint32_t primitiveCount = range == nullptr ? 0u : range->count;

      m_fieldObjectTable.makeGPUField(
        objectIndex,
        primitiveOffset,
        primitiveCount,
        fieldObject,
        m_frameContext.fieldRepresentation
      );

      if (fieldObject.dirty) {
        m_fieldObjectTable.appendToBakeList(objectIndex);
      }

      ++objectIndex;
    }

    m_frameContext.fieldObjectCount = objectIndex;

    m_scene.augmentFrameContext(m_frameContext);
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
      // Bake-list entries are packing slots, so this re-derives the same
      // span the loop above packed from. Nothing between the two mutates
      // the world, and a removal would reorder it: swap-and-pop storage.
      const std::span<const dunya::objectmodel::Entity> fieldEntities =
        world.fieldObjects();

      for (uint32_t idx : m_fieldObjectTable.bakeList()) {
        world.setDirty(fieldEntities[idx], false);
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
      for (dunya::objectmodel::Entity entity : world.fieldObjects()) {
        const dunya::objectmodel::FieldObject& fieldObject =
          registry.get<dunya::objectmodel::FieldObject>(entity);
        std::span<const dunya::field::Primitive> primitives =
          world.primitives(entity);
        dunya::renderer::VolumeImages images =
          m_volumePool.images(fieldObject.volumeIndex);
        m_fieldBaker.verifyBake(fieldObject, primitives, images);
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

  // The overlay gets first refusal on the cursor. Without this a click that
  // lands on a slider also carves the world behind it - the click reaches both,
  // because they are two systems reading the same button rather than one
  // passing it to the other.
  //
  // Not applied while looking: the cursor is captured then, its reported
  // position is virtual, and the overlay has no claim on a button being used to
  // fly.
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

/* Every panel this process shows, declared where the data it reads lives.
 *
 * They capture this, and the overlay outlives nothing that they touch: it is a
 * member of the class that owns everything they read.
 */
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

    size_t primitiveCount = m_scene.world().pool().size();

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
