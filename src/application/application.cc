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
}

Application::~Application() {
  EventDispatcher::instance().unsubscribe<KeyEvent>(m_keySubscription);
}

void Application::start() {
  glfwSetInputMode(
    m_context.window().handle(),
    GLFW_CURSOR,
    acceptsInput() ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL
  );

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

    bool curAcceptsInput = acceptsInput();

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

void Application::handleKeyEvent(const KeyEvent& event) {
  if (event.key == GLFW_KEY_ESCAPE && event.type == KeyEventType::Pressed) {
    m_input.toggleEnabled();

    clearCameraInput();

    glfwSetInputMode(
      m_context.window().handle(),
      GLFW_CURSOR,
      acceptsInput() ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL
    );
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
      *state = acceptsInput();
      break;

    case KeyEventType::Released:
      *state = false;
      break;

    default:
      break;
  }
}
