#include "application.ih"

Application::Application()
    : m_input(m_window.handle()),
      m_instance(validationLayers),
      m_surface(m_instance.handle(), m_window.handle()),
      m_device(m_instance.handle(), m_surface.handle()),
      m_swapChain(
        m_device.device(),
        m_device.physicalDevice(),
        m_surface.handle(),
        m_window.handle()
      ),
      m_depthImage(m_device, m_swapChain.swapChainExtent()),
      m_texture(m_device, "textures/viking_room.png"),
      m_mesh(m_device, "models/viking_room.obj"),
      m_descriptors(m_device, m_texture),
      m_pipeline(
        m_device.device(),
        m_swapChain.imageFormat(),
        m_descriptors,
        m_depthImage.format()
      ),
      m_renderer(
        m_device,
        m_surface.handle(),
        m_swapChain.imageCount(),
        m_mesh
      ),
      m_cameraInput({}),
      m_prevAcceptsInput(false)

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
    m_window.handle(),
    GLFW_CURSOR,
    acceptsInput() ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL
  );

  double prevTime = glfwGetTime();
  while (!glfwWindowShouldClose(m_window.handle())) {
    double now = glfwGetTime();
    float dt = static_cast<float>(now - prevTime);
    prevTime = now;

    glfwPollEvents();

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

    UniformBufferObject ubo{};
    ubo.model = glm::rotate(
      glm::mat4(1.0f),
      glm::radians(-90.0f),
      glm::vec3(1.0f, 0.0f, 0.0f)
    );

    ubo.view = m_camera.viewMatrix();

    float aspect = static_cast<float>(m_swapChain.swapChainExtent().width)
                   / static_cast<float>(m_swapChain.swapChainExtent().height);

    ubo.proj = m_camera.projectionMatrix(aspect);

    if (
      m_window.takeResized()
      || m_renderer.drawFrame(
        m_swapChain,
        m_pipeline,
        m_descriptors,
        m_depthImage,
        ubo
      )
    ) {
      m_swapChain.recreate();
      m_depthImage.recreate(m_device, m_swapChain.swapChainExtent());
    }
  }

  m_device.waitIdle();
}

void Application::clearCameraInput() noexcept {
  m_cameraInput = {};
}

bool Application::acceptsInput() const noexcept {
  return m_input.enabled() && m_window.focused();
}

void Application::handleKeyEvent(const KeyEvent& event) {
  if (event.key == GLFW_KEY_ESCAPE && event.type == KeyEventType::Pressed) {
    m_input.toggleEnabled();

    clearCameraInput();

    glfwSetInputMode(
      m_window.handle(),
      GLFW_CURSOR,
      acceptsInput() ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL
    );
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
