#include "application.ih"

Application::Application()
    : m_input(m_context.window().handle()),
      m_swapChain(m_context),
      m_scene(m_context),
      m_fieldPass(m_context.device(), Application::createPrimitives()),
      m_meshPipeline(
        PipelineType::Mesh,
        m_context.device().vkDevice(),
        m_scene.descriptors().setLayout(),
        m_swapChain
      ),
      m_fieldPipeline(
        PipelineType::Field,
        m_context.device().vkDevice(),
        m_fieldPass.setLayout(),
        m_swapChain
      ),
      m_renderer(
        m_context.device(),
        m_fieldPass,
        m_meshPipeline,
        m_fieldPipeline,
        m_scene.descriptors(),
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
  while (!glfwWindowShouldClose(m_context.window().handle())) {
    double now = glfwGetTime();
    float dt = static_cast<float>(now - prevTime);
    prevTime = now;

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

    m_frameContext.proj = m_camera.projectionMatrix(aspect);
    m_frameContext.view = m_camera.viewMatrix();
    m_frameContext.cameraPos = m_camera.position();
    m_scene.augmentFrameContext(m_frameContext);

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
    std::cout << "Pipeline mode switched to:" << (int)m_frameContext.mode
              << '\n';
  }

  if (event.key == GLFW_KEY_R && event.type == KeyEventType::SinglePressed) {
    m_reloadRequested = true;
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

std::vector<Primitive> Application::createPrimitives() {
  std::vector<Primitive> primitives;

  Primitive sphere{};
  sphere.inverseModel = glm::mat4(1.0f);
  sphere.shape = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
  sphere.shapeConfig = glm::uvec4(0, 0, 0, 0);

  Primitive box{};
  box.inverseModel = glm::inverse(
    glm::translate(glm::mat4(1.0f), glm::vec3(1.2f, 0.0f, 0.0f))
    * glm::rotate(
      glm::mat4(1.0f),
      glm::radians(30.0f),
      glm::vec3(0.0f, 1.0f, 0.0f)
    )
  );
  box.shape = glm::vec4(0.5f, 0.5f, 0.5f, 0.4f);
  box.shapeConfig = glm::uvec4(1, 0, 1, 0);

  Primitive plane{};
  plane.inverseModel =
    glm::inverse(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -2.0f, 0.0f)));
  plane.shape = glm::vec4(0.0f);
  plane.shapeConfig = glm::uvec4(2, 1, 0, 0);

  primitives.push_back(sphere);
  primitives.push_back(box);
  primitives.push_back(plane);

  return primitives;
}
