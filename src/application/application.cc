#include "application.ih"

Application::Application()
    : m_input(m_context.window().handle()),
      m_swapChain(m_context),
      m_scene(m_context),
      m_frameGlobals(m_context.device()),
      m_resourceTable(
        m_context.device(),
        m_scene.textures(),
        m_scene.samplers(),
        m_scene.materials()
      ),
      m_fieldPass(m_context.device(), m_scene.primitives()),
      m_meshPipeline(
        PipelineType::Mesh,
        m_context.device().vkDevice(),
        std::vector<VkDescriptorSetLayout>{
          m_frameGlobals.setLayout(),
          m_resourceTable.setLayout()
        },
        m_swapChain
      ),
      m_fieldPipeline(
        PipelineType::Field,
        m_context.device().vkDevice(),
        std::vector<VkDescriptorSetLayout>{
          m_frameGlobals.setLayout(),
          m_resourceTable.setLayout(),
          m_fieldPass.setLayout()
        },
        m_swapChain
      ),
      m_renderer(
        m_context.device(),
        m_fieldPass,
        m_frameGlobals,
        m_meshPipeline,
        m_fieldPipeline,
        m_resourceTable,
        m_context.surface().handle(),
        m_swapChain.imageCount()
      ),
      m_cameraInput({}),
      m_prevAcceptsInput(false),
      m_reloadRequested(false)

{
  m_keySubscription = EventDispatcher::instance().subscribe<KeyEvent>(
    [this](const KeyEvent& event) { handleKeyEvent(event); }
  );

  m_mouseSubscription = EventDispatcher::instance().subscribe<MouseButtonEvent>(
    [this](const MouseButtonEvent& event) { handleMouseButtonEvent(event); }
  );
}

Application::~Application() {
  EventDispatcher::instance().unsubscribe<MouseButtonEvent>(
    m_mouseSubscription
  );
  EventDispatcher::instance().unsubscribe<KeyEvent>(m_keySubscription);
}

void Application::start() {
  glfwSetInputMode(
    m_context.window().handle(),
    GLFW_CURSOR,
    GLFW_CURSOR_NORMAL
  );

  std::cout << "Hold right mouse to look and fly (WASD/QE)\n"
            << "Left click carves, shift + left click adds\n";

  double prevTime = glfwGetTime();
  double pipelineReloadCheck = 0;
  double statWindowStart = prevTime;
  uint32_t statFrames = 0;
  while (!glfwWindowShouldClose(m_context.window().handle())) {
    double now = glfwGetTime();
    float dt = static_cast<float>(now - prevTime);
    prevTime = now;

    ++statFrames;
    if (now - statWindowStart >= 1.0) {
      const double elapsed = now - statWindowStart;
      const double msPerFrame = (elapsed * 1000.0) / statFrames;

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

    // ---------- Frame context ----------
    m_frameContext.proj = m_camera.projectionMatrix(aspect);
    m_frameContext.view = m_camera.viewMatrix();
    m_frameContext.cameraPos = m_camera.position();
    m_scene.augmentFrameContext(m_frameContext);
    // -----------------------------------

    if (
      m_context.window().takeResized()
      || m_renderer.drawFrame(m_swapChain, m_frameContext)
    ) {
      m_swapChain.recreate();
    }
  }

  m_context.device().waitIdle();
}

void Application::clearCameraInput() noexcept {
  m_cameraInput = {};
}

bool Application::acceptsInput() const noexcept {
  return m_input.enabled() && m_context.window().focused();
}

void Application::handleMouseButtonEvent(const MouseButtonEvent& event) {
  if (!acceptsInput()) {
    return;
  }

  if (event.button == GLFW_MOUSE_BUTTON_RIGHT) {
    setLookMode(event.type == MouseButtonEventType::Pressed);
    return;
  }

  if (
    event.button != GLFW_MOUSE_BUTTON_LEFT
    || event.type != MouseButtonEventType::Pressed
  ) {
    return;
  }

  // Clicking is for the visible cursor; while looking, the reported position
  // is a virtual one that has nothing to do with the screen.
  if (m_looking) {
    return;
  }

  editField(
    (event.mods & GLFW_MOD_SHIFT) != 0 ? FIELD_OP_UNION : FIELD_OP_SUBTRACTION
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

void Application::editField(uint32_t operation) {
  const Cursor cursor = m_input.cursor();
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

  const dunya::field::Ray ray = dunya::field::screenPointToRay(
    glm::inverse(viewProj),
    glm::vec3(m_frameContext.cameraPos),
    ndc
  );

  const std::optional<dunya::field::RayHit> hit =
    dunya::field::raymarch(m_scene.primitives(), ray);

  if (!hit.has_value()) {
    std::cout << "Nothing under the cursor\n";
    return;
  }

  // Centred on the surface a carve only takes a shallow bite, because half the
  // sphere sits in empty space. Pushing it along the ray by its own radius puts
  // the whole sphere inside the material, so repeated clicks tunnel through.
  const glm::vec3 centre = operation == FIELD_OP_SUBTRACTION
                             ? hit->position + ray.direction * EDIT_RADIUS
                             : hit->position;

  // What the CPU thinks is there before the edit. At the surface this should
  // read about zero, which is the agreement between the ray the CPU marched
  // and the surface the shader drew.
  const float surfaceDistance =
    dunya::field::sample(m_scene.primitives(), hit->position).distance;
  const float centreBefore =
    dunya::field::sample(m_scene.primitives(), centre).distance;

  if (!m_scene.addPrimitive(centre, EDIT_RADIUS, hit->material, operation)) {
    std::cout << "Primitive budget full, edit refused\n";
    return;
  }

  const auto uploadStart = std::chrono::steady_clock::now();
  m_fieldPass.uploadPrimitives(m_scene.primitives());
  const auto uploadEnd = std::chrono::steady_clock::now();

  // A carve leaves empty space at its centre and an add leaves solid, so both
  // land on +/- the edit radius. Anything else means the CPU and the GPU
  // disagree about the array they share.
  const float centreAfter =
    dunya::field::sample(m_scene.primitives(), centre).distance;

  const auto uploadMicros =
    std::chrono::duration_cast<std::chrono::microseconds>(
      uploadEnd - uploadStart
    )
      .count();

  std::cout << (operation == FIELD_OP_SUBTRACTION ? "carve" : "add  ")
            << "  surface " << std::fixed << std::setprecision(4)
            << surfaceDistance << "  centre " << std::setprecision(3)
            << centreBefore << " -> " << centreAfter << "  upload "
            << uploadMicros << " us  primitives " << m_scene.primitives().size()
            << '\n';
}

void Application::handleKeyEvent(const KeyEvent& event) {
  if (event.key == GLFW_KEY_ESCAPE && event.type == KeyEventType::Pressed) {
    m_input.toggleEnabled();

    clearCameraInput();

    // The cursor now belongs to look mode, so escape leaves it rather than
    // setting the mode itself.
    setLookMode(false);
  }

  if (event.key == GLFW_KEY_P && event.type == KeyEventType::SinglePressed) {
    m_frameContext.mode = nextPipelineType(m_frameContext.mode);
    std::cout << "Pipeline mode switched to: " << (int)m_frameContext.mode
              << '\n';
  }

  if (event.key == GLFW_KEY_R && event.type == KeyEventType::SinglePressed) {
    m_reloadRequested = true;
    return;
  }

  if (event.key == GLFW_KEY_V && event.type == KeyEventType::SinglePressed) {
    m_context.device().waitIdle();
    m_swapChain.setUncapped(!m_swapChain.uncapped());
    std::cout << "Present mode: "
              << (m_swapChain.uncapped() ? "immediate" : "fifo/mailbox")
              << '\n';
    return;
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
    case KeyEventType::Pressed:
      // Movement belongs to look mode, the same way it does in a scene view.
      *state = acceptsInput() && m_looking;
      break;

    case KeyEventType::Released:
      *state = false;
      break;

    default:
      break;
  }
}
