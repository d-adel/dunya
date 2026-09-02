#include "engineloop.ih"

EngineLoop::EngineLoop(const StartupOptions& options)
    : m_engine(std::make_unique<GlfwWindowSystem>(m_window), options.project),
      m_input(m_window.handle()) {
  m_engine.loadWorld(options.world);

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

EngineLoop::~EngineLoop() {
  dunya::core::EventDispatcher::instance()
    .unsubscribe<dunya::platform::MouseButtonEvent>(m_mouseSubscription);
  dunya::core::EventDispatcher::instance()
    .unsubscribe<dunya::platform::KeyEvent>(m_keySubscription);
}

void EngineLoop::handleKeyEvent(const dunya::platform::KeyEvent& event) {
  if (
    const std::optional<bool> down = dunya::platform::keyTransition(event.type)
  ) {
    m_engine.input().setKey(dunya::app::keyFromGlfw(event.key), *down);
  }
}

void EngineLoop::handleMouseButtonEvent(
  const dunya::platform::MouseButtonEvent& event
) {
  m_engine.input().setMouseButton(
    dunya::app::buttonFromGlfw(event.button),
    event.type == dunya::platform::MouseButtonEventType::Pressed
  );
}

void EngineLoop::bindCamera() {
  const dunya::objectmodel::Entity eye =
    dunya::objectmodel::mainCamera(m_engine.activeWorld());

  m_port.camera = eye;
  m_port.mode = dunya::view::DrawMode::Both;

  if (eye == dunya::objectmodel::INVALID_ENTITY) {
    m_port.mode = dunya::view::DrawMode::Nothing;

    if (!m_reportedMissingCamera) {
      m_reportedMissingCamera = true;

      std::cout << "no camera in this world, so it draws nothing\n";
    }
  }

  if (!m_engine.configureViewport(m_viewport, m_port)) {
    throw std::runtime_error("the player's viewport went missing");
  }
}

bool EngineLoop::verifyBakes() {
  m_engine.context().device().waitIdle();

  dunya::objectmodel::World& world = m_engine.activeWorld();

  const entt::registry& registry = world.registry();

  uint32_t checked = 0;

  for (dunya::objectmodel::Entity entity : world.sdfGrids()) {
    const auto* volume =
      registry.try_get<dunya::objectmodel::BakedVolume>(entity);

    if (volume == nullptr) {
      continue;
    }

    m_engine.storage().sdfBaker().verifyBake(
      registry.get<dunya::objectmodel::SdfGrid>(entity),
      volume->index,
      world.primitives(entity),
      m_engine.storage().volumePool().images(volume->index)
    );

    ++checked;
  }

  return checked != 0;
}

int EngineLoop::start(const StartupOptions& options) {
  m_viewport = m_engine.createViewport();

  glfwSetInputMode(m_window.handle(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);

  if (!m_script.load("managed")) {
    throw std::runtime_error(
      "The script assembly did not load: " + m_script.lastError()
    );
  }

  if (!m_script.initialize(
        m_engine.schedule(),
        m_engine.world(),
        std::filesystem::path(options.project) / "scripts"
      )) {
    throw std::runtime_error(
      "The script assembly did not initialise: " + m_script.lastError()
    );
  }

  if (options.analytic) {
    m_port.fieldRepresentation = dunya::core::FIELD_ANALYTIC;
  }

  bool bakeCheckPending = options.verifyBake;

  FrameCheck frameCheck(m_engine.context(), m_engine.swapChain(), options);

  std::function<void(VkImage)> captureHook;

  if (frameCheck.wanted() || frameCheck.capturing()) {
    captureHook = [&frameCheck](VkImage image) {
      frameCheck.run(image);
    };
  }

  if (frameCheck.capturing()) {
    std::filesystem::create_directories(options.capture);
  }

  double prevTime = glfwGetTime();

  while (!glfwWindowShouldClose(m_window.handle())) {
    const double now = glfwGetTime();

    const float dt = frameCheck.capturing()
                       ? 1.0f / 60.0f
                       : static_cast<float>(now - prevTime);

    prevTime = now;

    glfwPollEvents();

    const dunya::platform::Cursor where = m_input.cursor();

    m_engine.input().setCursor(
      static_cast<float>(where.x),
      static_cast<float>(where.y)
    );

    constexpr uint32_t FIRST_PLAY_FRAME = 4u;

    if (
      m_engine.frameIndex() == FIRST_PLAY_FRAME && !frameCheck.wanted()
      && !m_engine.playing()
    ) {
      m_engine.play();
    }

    bindCamera();

    dunya::engine::FrameRequest request;
    request.viewport = m_viewport;
    request.deltaSeconds = dt;
    request.surfaceStale = m_window.takeResized();
    request.capture = captureHook;

    if (m_engine.renderFrame(request, m_telemetry)) {
      m_engine.resize();
    }

    if (bakeCheckPending) {
      bakeCheckPending = !verifyBakes();
    }

    if (frameCheck.ran()) {
      glfwSetWindowShouldClose(m_window.handle(), GLFW_TRUE);
    }
  }

  m_engine.context().device().waitIdle();

  if (bakeCheckPending) {
    std::cout << "bake check: nothing was verified" << std::endl;

    return 1;
  }

  return frameCheck.failed() ? 1 : 0;
}
