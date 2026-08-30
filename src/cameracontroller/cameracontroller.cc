#include "cameracontroller.ih"

CameraController::CameraController(
  dunya::platform::Input& input,
  dunya::platform::Window& window
)
    : m_input(input), m_window(window) {}

dunya::objectmodel::Camera& CameraController::camera() noexcept {
  return m_camera;
}

const dunya::objectmodel::Camera& CameraController::camera() const noexcept {
  return m_camera;
}

bool CameraController::looking() const noexcept {
  return m_looking;
}

void CameraController::clear() noexcept {
  m_state = {};
}

void CameraController::setLookMode(bool looking) {
  if (m_looking == looking) {
    return;
  }

  m_looking = looking;

  if (!m_looking) {
    clear();
  }

  m_input.cursorDeltaInvalid();

  glfwSetInputMode(
    m_window.handle(),
    GLFW_CURSOR,
    m_looking ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL
  );
}

bool CameraController::handleKey(
  const dunya::platform::KeyEvent& event,
  bool acceptsInput
) {
  bool* state = nullptr;

  switch (event.key) {
    case GLFW_KEY_W:
      state = &m_state.forward;
      break;

    case GLFW_KEY_S:
      state = &m_state.back;
      break;

    case GLFW_KEY_A:
      state = &m_state.left;
      break;

    case GLFW_KEY_D:
      state = &m_state.right;
      break;

    case GLFW_KEY_E:
      state = &m_state.up;
      break;

    case GLFW_KEY_Q:
      state = &m_state.down;
      break;

    default:
      return false;
  }

  switch (event.type) {
    case dunya::platform::KeyEventType::Pressed:
      *state = acceptsInput && m_looking;
      break;

    case dunya::platform::KeyEventType::Released:
      *state = false;
      break;

    default:
      break;
  }

  return true;
}

void CameraController::update(float dt, bool acceptsInput) {
  const bool accepting = acceptsInput && m_looking;

  if (accepting != m_wasAccepting) {
    m_input.cursorDeltaInvalid();
    m_wasAccepting = accepting;
  }

  m_input.update();

  if (!accepting) {
    return;
  }

  m_state.lookDx = static_cast<float>(m_input.cursor().dx);
  m_state.lookDy = static_cast<float>(m_input.cursor().dy);

  m_camera.update(dt, m_state);
}

dunya::field::Ray CameraController::cursorRay(
  VkExtent2D extent,
  const glm::mat4& viewProjection
) const {
  const dunya::platform::Cursor cursor = m_input.cursor();

  const glm::vec2 ndc(
    2.0f * static_cast<float>(cursor.x) / static_cast<float>(extent.width)
      - 1.0f,
    2.0f * static_cast<float>(cursor.y) / static_cast<float>(extent.height)
      - 1.0f
  );

  return dunya::field::screenPointToRay(
    glm::inverse(viewProjection),
    glm::vec3(m_camera.position()),
    ndc
  );
}
